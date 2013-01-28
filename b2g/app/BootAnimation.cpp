/* Copyright 2012 Mozilla Foundation and Mozilla contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <endian.h>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>
#include "mozilla/FileUtils.h"
#include "mozilla/NullPtr.h"
#include "mozilla/Util.h"
#include "png.h"

#include "android/log.h"
#include "ui/FramebufferNativeWindow.h"
#include "hardware_legacy/power.h"
#include "hardware/gralloc.h"

#define LOG(args...)  __android_log_print(ANDROID_LOG_INFO, "Gonk" , ## args)
#define LOGW(args...) __android_log_print(ANDROID_LOG_WARN, "Gonk", ## args)
#define LOGE(args...) __android_log_print(ANDROID_LOG_ERROR, "Gonk", ## args)

using namespace android;
using namespace mozilla;
using namespace std;

static sp<FramebufferNativeWindow> gNativeWindow;
static pthread_t sAnimationThread;
static bool sRunAnimation;

/* See http://www.pkware.com/documents/casestudies/APPNOTE.TXT */
struct local_file_header {
    uint32_t signature;
    uint16_t min_version;
    uint16_t general_flag;
    uint16_t compression;
    uint16_t lastmod_time;
    uint16_t lastmod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_size;
    uint16_t extra_field_size;
    char     data[0];

    uint32_t GetDataSize() const
    {
        return letoh32(uncompressed_size);
    }

    uint32_t GetSize() const
    {
        /* XXX account for data descriptor */
        return sizeof(local_file_header) + letoh16(filename_size) +
               letoh16(extra_field_size) + GetDataSize();
    }

    const char * GetData() const
    {
        return data + letoh16(filename_size) + letoh16(extra_field_size);
    }
} __attribute__((__packed__));

struct data_descriptor {
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
} __attribute__((__packed__));

struct cdir_entry {
    uint32_t signature;
    uint16_t creator_version;
    uint16_t min_version;
    uint16_t general_flag;
    uint16_t compression;
    uint16_t lastmod_time;
    uint16_t lastmod_date;
    uint32_t crc32;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    uint16_t filename_size;
    uint16_t extra_field_size;
    uint16_t file_comment_size;
    uint16_t disk_num;
    uint16_t internal_attr;
    uint32_t external_attr;
    uint32_t offset;
    char     data[0];

    uint32_t GetDataSize() const
    {
        return letoh32(compressed_size);
    }

    uint32_t GetSize() const
    {
        return sizeof(cdir_entry) + letoh16(filename_size) +
               letoh16(extra_field_size) + letoh16(file_comment_size);
    }

    bool Valid() const
    {
        return signature == htole32(0x02014b50);
    }
} __attribute__((__packed__));

struct cdir_end {
    uint32_t signature;
    uint16_t disk_num;
    uint16_t cdir_disk;
    uint16_t disk_entries;
    uint16_t cdir_entries;
    uint32_t cdir_size;
    uint32_t cdir_offset;
    uint16_t comment_size;
    char     comment[0];

    bool Valid() const
    {
        return signature == htole32(0x06054b50);
    }
} __attribute__((__packed__));

/* We don't have access to libjar and the zip reader in android
 * doesn't quite fit what we want to do. */
class ZipReader {
    const char *mBuf;
    const cdir_end *mEnd;
    const char *mCdir_limit;
    uint32_t mBuflen;

public:
    ZipReader() : mBuf(nullptr) {}
    ~ZipReader() {
        if (mBuf)
            munmap((void *)mBuf, mBuflen);
    }

    bool OpenArchive(const char *path)
    {
        int fd;
        do {
            fd = open(path, O_RDONLY);
        } while (fd == -1 && errno == EINTR);
        if (fd == -1)
            return false;

        struct stat sb;
        if (fstat(fd, &sb) == -1 || sb.st_size < sizeof(cdir_end)) {
            close(fd);
            return false;
        }

        mBuflen = sb.st_size;
        mBuf = (char *)mmap(nullptr, sb.st_size, PROT_READ, MAP_SHARED, fd, 0);
        close(fd);

        if (!mBuf) {
            return false;
        }

        madvise(mBuf, sb.st_size, MADV_SEQUENTIAL);

        mEnd = (cdir_end *)(mBuf + mBuflen - sizeof(cdir_end));
        while (!mEnd->Valid() &&
               (char *)mEnd > mBuf) {
            mEnd = (cdir_end *)((char *)mEnd - 1);
        }

        mCdir_limit = mBuf + letoh32(mEnd->cdir_offset) + letoh32(mEnd->cdir_size);

        if (!mEnd->Valid() || mCdir_limit > (char *)mEnd) {
            munmap((void *)mBuf, mBuflen);
            mBuf = nullptr;
            return false;
        }

        return true;
    }

    /* Pass null to get the first cdir entry */
    const cdir_entry * GetNextEntry(const cdir_entry *prev)
    {
        const cdir_entry *entry;
        if (prev)
            entry = (cdir_entry *)((char *)prev + prev->GetSize());
        else
            entry = (cdir_entry *)(mBuf + letoh32(mEnd->cdir_offset));

        if (((char *)entry + entry->GetSize()) > mCdir_limit ||
            !entry->Valid())
            return nullptr;
        return entry;
    }

    string GetEntryName(const cdir_entry *entry)
    {
        uint16_t len = letoh16(entry->filename_size);

        string name;
        name.append(entry->data, len);
        return name;
    }

    const local_file_header * GetLocalEntry(const cdir_entry *entry)
    {
        const local_file_header * data =
            (local_file_header *)(mBuf + letoh32(entry->offset));
        if (((char *)data + data->GetSize()) > (char *)mEnd)
            return nullptr;
        return data;
    }
};

struct AnimationFrame {
    char path[256];
    char *buf;
    const local_file_header *file;
    uint32_t width;
    uint32_t height;
    uint16_t bytepp;

    AnimationFrame() : buf(nullptr) {}
    AnimationFrame(const AnimationFrame &frame) : buf(nullptr) {
        strncpy(path, frame.path, sizeof(path));
        file = frame.file;
    }
    ~AnimationFrame()
    {
        if (buf)
            free(buf);
    }

    bool operator<(const AnimationFrame &other) const
    {
        return strcmp(path, other.path) < 0;
    }

    void ReadPngFrame(int outputFormat);
};

struct AnimationPart {
    int32_t count;
    int32_t pause;
    char path[256];
    vector<AnimationFrame> frames;
};

using namespace android;

struct RawReadState {
    const char *start;
    uint32_t offset;
    uint32_t length;
};

static void
RawReader(png_structp png_ptr, png_bytep data, png_size_t length)
{
    RawReadState *state = (RawReadState *)png_get_io_ptr(png_ptr);
    if (length > (state->length - state->offset))
        png_err(png_ptr);

    memcpy(data, state->start + state->offset, length);
    state->offset += length;
}

static void
TransformTo565(png_structp png_ptr, png_row_infop row_info, png_bytep data)
{
    uint16_t *outbuf = (uint16_t *)data;
    uint8_t *inbuf = (uint8_t *)data;
    for (int i = 0; i < row_info->rowbytes; i += 3) {
        *outbuf++ = ((inbuf[i]     & 0xF8) << 8) |
                    ((inbuf[i + 1] & 0xFC) << 3) |
                    ((inbuf[i + 2]       ) >> 3);
    }
}

void
AnimationFrame::ReadPngFrame(int outputFormat)
{
    png_structp pngread = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 nullptr, nullptr, nullptr);

    if (!pngread)
        return;

    png_infop pnginfo = png_create_info_struct(pngread);

    if (!pnginfo) {
        png_destroy_read_struct(&pngread, &pnginfo, nullptr);
        return;
    }

    if (setjmp(png_jmpbuf(pngread))) {
        // libpng reported an error and longjumped here.  Clean up and return.
        png_destroy_read_struct(&pngread, &pnginfo, nullptr);
        return;
    }

    RawReadState state;
    state.start = file->GetData();
    state.length = file->GetDataSize();
    state.offset = 0;

    png_set_read_fn(pngread, &state, RawReader);

    png_read_info(pngread, pnginfo);

    width = png_get_image_width(pngread, pnginfo);
    height = png_get_image_height(pngread, pnginfo);
    switch (outputFormat) {
    case HAL_PIXEL_FORMAT_BGRA_8888:
        png_set_bgr(pngread);
        // FALL THROUGH
    case HAL_PIXEL_FORMAT_RGBA_8888:
    case HAL_PIXEL_FORMAT_RGBX_8888:
        bytepp = 4;
        png_set_filler(pngread, 0xFF, PNG_FILLER_AFTER);
        break;
    case HAL_PIXEL_FORMAT_RGB_888:
        bytepp = 3;
        png_set_strip_alpha(pngread);
        break;
    default:
        LOGW("Unknown pixel format %d. Assuming RGB 565.", outputFormat);
        // FALL THROUGH
    case HAL_PIXEL_FORMAT_RGB_565:
        bytepp = 2;
        png_set_strip_alpha(pngread);
        png_set_read_user_transform_fn(pngread, TransformTo565);
        break;
    }

    // An extra row is added to give libpng enough space when
    // decoding 3/4 bytepp inputs for 2 bytepp output surfaces
    buf = (char *)malloc(width * (height + 1) * bytepp);

    vector<char *> rows(height + 1);
    uint32_t stride = width * bytepp;
    for (int i = 0; i < height; i++) {
        rows[i] = buf + (stride * i);
    }
    rows[height] = nullptr;
    png_set_strip_16(pngread);
    png_set_palette_to_rgb(pngread);
    png_read_image(pngread, (png_bytepp)&rows.front());
    png_destroy_read_struct(&pngread, &pnginfo, nullptr);
}

static void *
AnimationThread(void *)
{
    ZipReader reader;
    if (!reader.OpenArchive("/system/media/bootanimation.zip")) {
        LOGW("Could not open boot animation");
        return nullptr;
    }

    const cdir_entry *entry = nullptr;
    const local_file_header *file = nullptr;
    while ((entry = reader.GetNextEntry(entry))) {
        string name = reader.GetEntryName(entry);
        if (!name.compare("desc.txt")) {
            file = reader.GetLocalEntry(entry);
            break;
        }
    }

    if (!file) {
        LOGW("Could not find desc.txt in boot animation");
        return nullptr;
    }

    int format;
    ANativeWindow *window = gNativeWindow.get();
    window->query(window, NATIVE_WINDOW_FORMAT, &format);

    hw_module_t const *module;
    if (hw_get_module(GRALLOC_HARDWARE_MODULE_ID, &module)) {
        LOGW("Could not get gralloc module");
        return nullptr;
    }
    gralloc_module_t const *grmodule =
        reinterpret_cast<gralloc_module_t const*>(module);

    string descCopy;
    descCopy.append(file->GetData(), entry->GetDataSize());
    int32_t width, height, fps;
    const char *line = descCopy.c_str();
    const char *end;
    bool headerRead = true;
    vector<AnimationPart> parts;

    /*
     * bootanimation.zip
     *
     * This is the boot animation file format that Android uses.
     * It's a zip file with a directories containing png frames
     * and a desc.txt that describes how they should be played.
     *
     * desc.txt contains two types of lines
     * 1. [width] [height] [fps]
     *    There is one of these lines per bootanimation.
     *    If the width and height are smaller than the screen,
     *    the frames are centered on a black background.
     *    XXX: Currently we stretch instead of centering the frame.
     * 2. p [count] [pause] [path]
     *    This describes one animation part.
     *    Each animation part is played in sequence.
     *    An animation part contains all the files/frames in the
     *    directory specified in [path]
     *    [count] indicates the number of times this part repeats.
     *    [pause] indicates the number of frames that this part
     *    should pause for after playing the full sequence but
     *    before repeating.
     */

    do {
        end = strstr(line, "\n");

        AnimationPart part;
        if (headerRead &&
            sscanf(line, "%d %d %d", &width, &height, &fps) == 3) {
            headerRead = false;
        } else if (sscanf(line, "p %d %d %s",
                          &part.count, &part.pause, part.path)) {
            parts.push_back(part);
        }
    } while (end && *(line = end + 1));

    for (uint32_t i = 0; i < parts.size(); i++) {
        AnimationPart &part = parts[i];
        entry = nullptr;
        char search[256];
        snprintf(search, sizeof(search), "%s/", part.path);
        while ((entry = reader.GetNextEntry(entry))) {
            string name = reader.GetEntryName(entry);
            if (name.find(search) ||
                !entry->GetDataSize() ||
                name.length() >= 256)
                continue;

            part.frames.push_back();
            AnimationFrame &frame = part.frames.back();
            strcpy(frame.path, name.c_str());
            frame.file = reader.GetLocalEntry(entry);
        }

        sort(part.frames.begin(), part.frames.end());
    }

    uint32_t frameDelayUs = 1000000 / fps;

    for (uint32_t i = 0; i < parts.size(); i++) {
        AnimationPart &part = parts[i];

        uint32_t j = 0;
        while (sRunAnimation && (!part.count || j++ < part.count)) {
            for (uint32_t k = 0; k < part.frames.size(); k++) {
                struct timeval tv1, tv2;
                gettimeofday(&tv1, nullptr);
                AnimationFrame &frame = part.frames[k];
                if (!frame.buf) {
                    frame.ReadPngFrame(format);
                }

                ANativeWindowBuffer *buf;
                if (window->dequeueBuffer(window, &buf)) {
                    LOGW("Failed to get an ANativeWindowBuffer");
                    break;
                }
                if (window->lockBuffer(window, buf)) {
                    LOGW("Failed to lock ANativeWindowBuffer");
                    window->queueBuffer(window, buf);
                    break;
                }

                void *vaddr;
                if (grmodule->lock(grmodule, buf->handle,
                                   GRALLOC_USAGE_SW_READ_NEVER |
                                   GRALLOC_USAGE_SW_WRITE_OFTEN |
                                   GRALLOC_USAGE_HW_FB,
                                   0, 0, width, height, &vaddr)) {
                    LOGW("Failed to lock buffer_handle_t");
                    window->queueBuffer(window, buf);
                    break;
                }
                memcpy(vaddr, frame.buf,
                       frame.width * frame.height * frame.bytepp);
                grmodule->unlock(grmodule, buf->handle);

                gettimeofday(&tv2, nullptr);

                timersub(&tv2, &tv1, &tv2);

                if (tv2.tv_usec < frameDelayUs) {
                    usleep(frameDelayUs - tv2.tv_usec);
                } else {
                    LOGW("Frame delay is %d us but decoding took %d us", frameDelayUs, tv2.tv_usec);
                }

                window->queueBuffer(window, buf);

                if (part.count && j >= part.count) {
                    free(frame.buf);
                    frame.buf = nullptr;
                }
            }
            usleep(frameDelayUs * part.pause);
        }
    }

    return nullptr;
}

static int
CancelBufferNoop(ANativeWindow* aWindow, android_native_buffer_t* aBuffer)
{
    return 0;
}

__attribute__ ((visibility ("default")))
FramebufferNativeWindow*
NativeWindow()
{
    if (gNativeWindow.get()) {
        return gNativeWindow.get();
    }

    // Some gralloc HALs need this in order to open the
    // framebuffer device after we restart with the screen off.
    //
    // NB: this *must* run BEFORE allocating the
    // FramebufferNativeWindow.  Do not separate these two C++
    // statements.
    set_screen_state(1);

    // For some devices, it takes a while for the framebuffer to become
    // usable. So we wait until the framebuffer has woken up before we
    // try to open it.
    {
        char buf;
        int len = 0;
        ScopedClose fd(open("/sys/power/wait_for_fb_wake", O_RDONLY, 0));
        do {
            len = read(fd.get(), &buf, 1);
        } while (len < 0 && errno == EINTR);
        if (len < 0) {
            LOGE("BootAnimation: wait_for_fb_sleep failed errno: %d", errno);
        }
    }

    // We (apparently) don't have a way to tell if allocating the
    // fbs succeeded or failed.
    gNativeWindow = new FramebufferNativeWindow();

    // Bug 776742: FrambufferNativeWindow doesn't set the cancelBuffer
    // function pointer, causing EGL to segfault when the window surface
    // is destroyed (i.e. on process exit). This workaround stops us
    // from hard crashing in that situation.
    gNativeWindow->cancelBuffer = CancelBufferNoop;

    sRunAnimation = true;
    pthread_create(&sAnimationThread, nullptr, AnimationThread, nullptr);

    return gNativeWindow.get();
}


__attribute__ ((visibility ("default")))
void
StopBootAnimation()
{
    if (sRunAnimation) {
        sRunAnimation = false;
        pthread_join(sAnimationThread, nullptr);
    }
}
