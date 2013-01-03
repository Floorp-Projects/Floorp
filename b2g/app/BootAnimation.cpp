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

#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <algorithm>
#include <endian.h>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <vector>
#include "mozilla/Util.h"
#include "mozilla/NullPtr.h"
#include "png.h"

#include "android/log.h"
#include "ui/FramebufferNativeWindow.h"
#include "hardware_legacy/power.h"

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
    uint16_t width;
    uint16_t height;
    const local_file_header *file;

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

    void ReadPngFrame();
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

void
AnimationFrame::ReadPngFrame()
{
    png_structp pngread = png_create_read_struct(PNG_LIBPNG_VER_STRING,
                                                 nullptr, nullptr, nullptr);

    png_infop pnginfo = png_create_info_struct(pngread);

    RawReadState state;
    state.start = file->GetData();
    state.length = file->GetDataSize();
    state.offset = 0;

    png_set_read_fn(pngread, &state, RawReader);

    setjmp(png_jmpbuf(pngread));

    png_read_info(pngread, pnginfo);

    width = png_get_image_width(pngread, pnginfo);
    height = png_get_image_height(pngread, pnginfo);
    buf = (char *)malloc(width * height * 3);

    vector<char *> rows(height + 1);
    uint32_t stride = width * 3;
    for (int i = 0; i < height; i++) {
        rows[i] = buf + (stride * i);
    }
    rows[height] = nullptr;
    png_set_palette_to_rgb(pngread);
    png_read_image(pngread, (png_bytepp)&rows.front());
    png_destroy_read_struct(&pngread, &pnginfo, nullptr);
}

static const EGLint kEGLConfigAttribs[] = {
    EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
    EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
    EGL_NONE
};

static bool
CreateConfig(EGLConfig* aConfig, EGLDisplay display, int format)
{
    EGLConfig configs[64];
    EGLint ncfg = ArrayLength(configs);

    if (!eglChooseConfig(display, kEGLConfigAttribs,
                         configs, ncfg, &ncfg) ||
        ncfg < 1) {
        return false;
    }

    for (int j = 0; j < ncfg; ++j) {
        EGLConfig config = configs[j];
        EGLint id;

        if (eglGetConfigAttrib(display, config,
                               EGL_NATIVE_VISUAL_ID, &id) &&
            id > 0 && id == format)
        {
            *aConfig = config;
            return true;
        }
    }
    return false;
}

static void *
AnimationThread(void *)
{
    ZipReader reader;
    if (!reader.OpenArchive("/system/media/bootanimation.zip")) {
        LOGW("Could not open boot animation");
        return nullptr;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    eglInitialize(display, nullptr, nullptr);

    int format;
    ANativeWindow const * const window = gNativeWindow.get();
    window->query(window, NATIVE_WINDOW_FORMAT, &format);

    EGLConfig config;
    if (!CreateConfig(&config, display, format)) {
        LOGW("Could not find config for pixel format");
        return nullptr;
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, gNativeWindow.get(), nullptr);

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

    static EGLint gContextAttribs[] = {
        EGL_CONTEXT_CLIENT_VERSION, 2,
        EGL_NONE, 0
    };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, gContextAttribs);

    eglMakeCurrent(display, surface, surface, context);
    glEnable(GL_TEXTURE_2D);

    const char *vsString =
        "attribute vec2 aPosition; "
        "attribute vec2 aTexCoord; "
        "varying vec2 vTexCoord; "
        "void main() { "
        "  gl_Position = vec4(aPosition, 0.0, 1.0); "
        "  vTexCoord = aTexCoord; "
        "}";

    const char *fsString =
        "precision mediump float; "
        "varying vec2 vTexCoord; "
        "uniform sampler2D sTexture; "
        "void main() { "
        "  gl_FragColor = vec4(texture2D(sTexture, vTexCoord).rgb, 1.0); "
        "}";

    GLint status;
    GLuint vsh = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vsh, 1, &vsString, nullptr);
    glCompileShader(vsh);
    glGetShaderiv(vsh, GL_COMPILE_STATUS, &status);
    if (!status) {
        LOGE("Failed to compile vertex shader");
        return nullptr;
    }

    GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fsh, 1, &fsString, nullptr);
    glCompileShader(fsh);
    glGetShaderiv(fsh, GL_COMPILE_STATUS, &status);
    if (!status) {
        LOGE("Failed to compile fragment shader");
        return nullptr;
    }

    GLuint programId = glCreateProgram();
    glAttachShader(programId, vsh);
    glAttachShader(programId, fsh);

    glLinkProgram(programId);
    glGetProgramiv(programId, GL_LINK_STATUS, &status);
    if (!status) {
        LOG("Failed to link program");
        return nullptr;
    }

    GLint positionLoc = glGetAttribLocation(programId, "aPosition");
    GLint texCoordLoc = glGetAttribLocation(programId, "aTexCoord");
    GLint textureLoc = glGetUniformLocation(programId, "sTexture");

    glUseProgram(programId);

    GLfloat texCoords[] = { 0.0f, 1.0f,
                            0.0f, 0.0f,
                            1.0f, 1.0f,
                            1.0f, 0.0f };

    GLfloat vCoords[] = { -1.0f, -1.0f,
                          -1.0f,  1.0f,
                           1.0f, -1.0f,
                           1.0f,  1.0f };

    GLuint rectBuf, texBuf;
    glGenBuffers(1, &rectBuf);
    glGenBuffers(1, &texBuf);

    GLuint tex;
    glGenTextures(1, &tex);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glEnableVertexAttribArray(positionLoc);
    glBindBuffer(GL_ARRAY_BUFFER, rectBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vCoords), vCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(positionLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(texCoordLoc);
    glBindBuffer(GL_ARRAY_BUFFER, texBuf);
    glBufferData(GL_ARRAY_BUFFER, sizeof(texCoords), texCoords, GL_STATIC_DRAW);
    glVertexAttribPointer(texCoordLoc, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glUniform1i(textureLoc, 0);

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
                    frame.ReadPngFrame();
                }

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB,
                             frame.width, frame.height, 0,
                             GL_RGB, GL_UNSIGNED_BYTE, frame.buf);
                glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

                gettimeofday(&tv2, nullptr);

                timersub(&tv2, &tv1, &tv2);

                if (tv2.tv_usec < frameDelayUs) {
                    usleep(frameDelayUs - tv2.tv_usec);
                } else {
                    LOGW("Frame delay is %d us but decoding took %d us", frameDelayUs, tv2.tv_usec);
                }

                eglSwapBuffers(display, surface);

                if (part.count && j >= part.count) {
                    free(frame.buf);
                    frame.buf = nullptr;
                }
            }
            usleep(frameDelayUs * part.pause);
        }
    }
    glBindTexture(GL_TEXTURE_2D, 0);
    glUseProgram(0);
    glDeleteTextures(1, &tex);
    glDeleteBuffers(1, &texBuf);
    glDeleteBuffers(1, &rectBuf);
    glDeleteProgram(programId);
    glDeleteShader(fsh);
    glDeleteShader(vsh);

    eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);

    eglDestroyContext(display, context);
    eglDestroySurface(display, surface);
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
