/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include <stdlib.h>
#include <string.h>

#include "plstr.h"
#include "prprf.h"

#include "ZipArchive.h"
#include "CUtils.h"
#include "StringUtils.h"
#include "LogModule.h"
#include "MemoryAccess.h"

#include "zlib.h"


UT_DEFINE_LOG_MODULE(ZipArchive);

/* External layout of various zip file records.  */

struct External_LocalFileHeader
{
  char signature[4];				            // 'PK\003\004'
  char version_needed_to_extract[2];
  char general_purpose_bit_flag[2];
  char compression_method[2];
  char last_mod_file_time[2];
  char last_mod_file_date[2];
  char crc32[4];
  char compressed_size[4];
  char uncompressed_size[4];
  char filename_length[2];
  char extra_field_length[2];
};

struct External_CentralDirectoryHeader
{
  char signature[4];				            // 'PK\001\002'
  char version_made_by[2];
  char version_needed_to_extract[2];
  char general_purpose_bit_flag[2];
  char compression_method[2];
  char last_mod_file_time[2];
  char last_mod_file_date[2];
  char crc32[4];
  char compressed_size[4];
  char uncompressed_size[4];
  char filename_length[2];
  char extra_field_length[2];
  char file_comment_length[2];
  char disk_number_start[2];
  char internal_file_attributes[2];
  char external_file_attributes[4];
  char relative_offset_local_header[4];
};

struct External_EndCentralDirectoryHeader
{
  char signature[4];				            // 'PK\005\006'
  char number_this_disk[2];
  char num_disk_with_start_central_dir[2];
  char num_entries_central_dir_this_disk[2];
  char total_entries_central_dir[2];
  char size_central_directory[4];
  char offset_start_central_directory[4];
  char zipfile_comment_length[2];
};

struct Internal_LocalFileHeader
{
  uint8 signature[4];
  uint16 version_needed_to_extract;
  uint16 general_purpose_bit_flag;
  uint16 compression_method;
  uint16 last_mod_file_time;
  uint16 last_mod_file_date;
  uint32 crc32;
  uint32 compressed_size;
  uint32 uncompressed_size;
  uint16 filename_length;
  uint16 extra_field_length;
};

struct Internal_CentralDirectoryHeader
{
  uint8 signature[4];
  uint16 version_made_by;
  uint16 version_needed_to_extract;
  uint16 general_purpose_bit_flag;
  uint16 compression_method;
  uint16 last_mod_file_time;
  uint16 last_mod_file_date;
  uint32 crc32;
  uint32 compressed_size;
  uint32 uncompressed_size;
  uint16 filename_length;
  uint16 extra_field_length;
  uint16 file_comment_length;
  uint16 disk_number_start;
  uint16 internal_file_attributes;
  uint32 external_file_attributes;
  uint32 relative_offset_local_header;
};

struct Internal_EndCentralDirectoryHeader
{
  uint8 signature[4];
  uint16 number_this_disk;
  uint16 num_disk_with_start_central_dir;
  uint16 num_entries_central_dir_this_disk;
  uint16 total_entries_central_dir;
  uint32 size_central_directory;
  uint32 offset_start_central_directory;
  uint16 zipfile_comment_length;
};

static void import(Internal_LocalFileHeader *,
                   const External_LocalFileHeader &);
static void import(Internal_CentralDirectoryHeader *,
                   const External_CentralDirectoryHeader &);
static void import(Internal_EndCentralDirectoryHeader *,
                   const External_EndCentralDirectoryHeader &);

#define STORED            0    /* compression methods */
#define SHRUNK            1
#define REDUCED1          2
#define REDUCED2          3
#define REDUCED3          4
#define REDUCED4          5
#define IMPLODED          6
#define TOKENIZED         7
#define DEFLATED          8


/* Public functions */
ZipArchive::ZipArchive(const char *archiveName, Pool &pool, bool &status)
  : pool(pool)
{
  status = true;

  fp = PR_Open(archiveName, PR_RDONLY, 0);
  if (!fp || !initReader())
    status = false;
}


ZipArchive::~ZipArchive()
{
  if (fp)
    PR_Close(fp);
}

bool ZipArchive::get(const DirectoryEntry *dp, char *&buf, Int32 &len)
{
  char *xbuf;
  int32 offset;
  External_LocalFileHeader ext_fhdr;
  Internal_LocalFileHeader int_fhdr;

  // Seek to the start of the local file header
  if (PR_Seek (fp, dp->off, PR_SEEK_SET) != dp->off)
    return false;

  // Read the local file header into memory
  if (!readFully(&ext_fhdr, sizeof(External_LocalFileHeader)))
    return false;

  // Swap endianness, if needed
  import(&int_fhdr, ext_fhdr);

  // Verify local file header signature, in case of corruption or confusion
  if (int_fhdr.signature[0] != 0x50 || int_fhdr.signature[1] != 0x4b ||
      int_fhdr.signature[2] != 0x03 || int_fhdr.signature[3] != 0x04)
    return false;

  // Compute offset within archive to start of file data
  offset = dp->off + sizeof(External_LocalFileHeader) + 
        int_fhdr.filename_length + int_fhdr.extra_field_length;

  // Seek to start of file data
  if (PR_Seek (fp, offset, PR_SEEK_SET) != offset)
    return false;

  len = dp->len;
  buf = xbuf = new(pool) char[dp->len];

  if (dp->method == STORED)
    return readFully(xbuf, dp->len);

  if (dp->method == DEFLATED)
    return inflateFully(dp->size, xbuf, dp->len);

  return false;
}
  
Uint32 ZipArchive::getNumElements(const char *fn_suffix)
{
  int suf_len = strlen(fn_suffix);
  Uint32 nelems = 0;
  DirectoryEntry *entry;

  entry = dir.firstNode();
  while (entry) {
    const char *fn = entry->fn;
    int fn_len = strlen(fn);
    if (suf_len >= fn_len
        && PL_strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0) {
      nelems++;
    }
    entry = dir.next(entry);
  }

  return nelems;
}
  
Uint32 ZipArchive::listElements(const char *fn_suffix, char **&rbuf)
{
  int suf_len = strlen(fn_suffix);
  Uint32 nelems;
  Uint32 i;
  char **buf = new (pool) char *[nel];
  DirectoryEntry *entry;

  rbuf = buf;
  entry = dir.firstNode();

  for (i=0, nelems=0; i < nel; i++) {
    const char *fn = entry->fn;
    int fn_len = strlen(fn);
    if (fn_len >= suf_len
        && PL_strncasecmp(&fn[fn_len - suf_len], fn_suffix, suf_len) == 0) {
      buf[nelems] = dupString(fn, pool);
      nelems++;
    }
    entry = dir.next(entry);
  }

  return nelems;
}

/* Private Functions */
/*
 * Initialize zip file reader, read in central directory and construct the
 * lookup table for locating zip file members.
 */
bool ZipArchive::initReader()
{
  External_EndCentralDirectoryHeader ext_ehdr;
  Internal_EndCentralDirectoryHeader int_ehdr;
  char *central_directory, *cd_ptr;
  DirectoryEntry *edir;
  PRInt32 pos;
  
  if (! findEnd())
    return false;
  if (! readFully(&ext_ehdr, sizeof(ext_ehdr)))
    return false;

  // Byte-swap the header as necessary.
  import(&int_ehdr, ext_ehdr);

  cenoff = int_ehdr.offset_start_central_directory;

  // Block read the entire central directory.  Add one byte to make sure
  // we can null-terminate the last filename in the directory.

  central_directory = new (pool) char[int_ehdr.size_central_directory + 1];
  if (!central_directory)
    return false;
  pos = PR_Seek(fp, int_ehdr.offset_start_central_directory, PR_SEEK_SET);
  if (pos == -1)
    return false;
  if (! readFully(central_directory, int_ehdr.size_central_directory))
    return false;

  // Import all of the individual directory entries.

  nel = int_ehdr.total_entries_central_dir;
  edir = new (pool) DirectoryEntry[nel];
  cd_ptr = central_directory;

  for (int i = 0; i < nel; ++i) {
    Internal_CentralDirectoryHeader int_chdr;
    import(&int_chdr, *(External_CentralDirectoryHeader*)cd_ptr);

    edir[i].len = int_chdr.uncompressed_size;
    edir[i].size = int_chdr.compressed_size;
    edir[i].method = int_chdr.compression_method;
    edir[i].mod = 0; // FIXME
    edir[i].off = int_chdr.relative_offset_local_header;

    cd_ptr += sizeof(External_CentralDirectoryHeader);
    edir[i].fn = cd_ptr;
    cd_ptr += int_chdr.filename_length;

    // Null terminate the filename at the expense of either the 
    // extra_field_length padding, or the signature on the next
    // entry, for which we care not at all.
    //
    // FIXME -- do we care about transforming filenames as infozip
    // would have us do in the general case?

    *cd_ptr = '\0';

    cd_ptr += int_chdr.extra_field_length;
      
    // Sanity check the entry.  
    if (cd_ptr - central_directory > int_ehdr.size_central_directory)
      return false;

    // Insert the entry into the tree.
    dir.insert(edir[i]);
  }

  return true;
}

/*
 * Find end of central directory (END) header in zip file and set the file
 * pointer to start of header. Return FALSE if the END header could not be
 * found, otherwise return TRUE.
 */
bool ZipArchive::findEnd()
{
  char buf[BUFSIZ+4];
  PRInt32 pos, dest, n;
  
  pos = PR_Seek(fp, 0, PR_SEEK_END);
  if (pos == -1)
    return false;
  if (pos < sizeof(External_EndCentralDirectoryHeader))
    return false;

  // Loop through the blocks of the file, starting at the end and going
  // toward the beginning.

  // Align our block reading.
  n = pos % BUFSIZ;
  if (!n)
    n = BUFSIZ;
  dest = pos - n;

  // The signature cannot be closer to the end than the size of the struct.
  n -= sizeof(External_EndCentralDirectoryHeader)-4;
  memset(buf+n, 0, 4);

  do {
    pos = PR_Seek(fp, dest, PR_SEEK_SET);
    if (!readFully(buf, n))
      return false;

    // Ah for a memrchr...
    while (--n >= 0)
      if (buf[n] == 'P' && buf[n+1] == 'K' && buf[n+2] == 5 && buf[n+3] == 6) {
        // Found it!
        endoff = pos + n;
        return PR_Seek(fp, endoff, PR_SEEK_SET) == endoff;
      }

    // The signature might span multiple blocks.  Arrange for the first
    // few bits of the current block to be adjacent to the last few bits
    // of the previous block.
    memcpy(buf+BUFSIZ, buf, 4);

    // Back up for the next block.
    dest = pos - BUFSIZ;
    n = BUFSIZ;
  } while (pos > 0);

  return false;
}


/*
 * Read exactly 'len' bytes of data into 'buf'. Return true if all bytes
 * could be read, otherwise return false.
 */
bool ZipArchive::readFully(void *buf, Int32 len)
{
  char *bp = (char *) buf;

  while (len > 0) {
    Int32 n = PR_Read(fp, bp, len);

    if (n <= 0) 
      return false;
    
    bp += n;
    len -= n;
  }

  return true;
}

/* Fully inflate the zip archive into buffer buf, whose length is len. 
 * Return true on success, false on failure.
 */
bool ZipArchive::inflateFully(Uint32 size, void *outbuf, Uint32 len)
{
  char inbuf[BUFSIZ];
  z_stream zs;
  bool success;
  Uint32 n;
  
  // Read the first data block for zlib.
  n = size < BUFSIZ ? size : BUFSIZ;
  if (!readFully(inbuf, n))
    return false;

  // Set up the z_stream.
  zs.next_in = (Bytef *) inbuf;
  zs.avail_in = n;
  zs.next_out = (Bytef *) outbuf;
  zs.avail_out = len;
  zs.zalloc = 0;
  zs.zfree = 0;
  zs.opaque = 0;

  // windowBits is passed < 0 to tell that there is no zlib header.
  // Note that in this case inflate *requires* an extra "dummy" byte
  // after the compressed stream in order to complete decompression and
  // return Z_STREAM_END.

  if (inflateInit2(&zs, -MAX_WBITS) != Z_OK)
    return false;
  
  success = true;
  while (1) {
    size -= n;

    // Process the block.  If size == 0, we know we're on the last hunk.
    if (size != 0) {
      int ret = inflate(&zs, 0);
      if (ret != Z_OK) {
	success = false;
	break;
      }
    }
    else {
      // Add the extra dummy byte mentioned above.  Just steal it from
      // the surrounding stack frame.
      zs.avail_in += 1;

      int ret = inflate(&zs, Z_FINISH);
      if (ret != Z_STREAM_END)
	success = false;
      break;
    }

    n = size < BUFSIZ ? size : BUFSIZ;
    if (!readFully(inbuf, n)) {
      success = false;
      break;
    }
    
    zs.next_in = (Bytef *) inbuf;
    zs.avail_in = n;
  }

  inflateEnd(&zs);
  return success;
}

static void
import(Internal_LocalFileHeader *in,
       const External_LocalFileHeader &ex)
{
  memcpy(in->signature, ex.signature, 4);
  in->version_needed_to_extract
    = readLittleUHalfwordUnaligned(ex.version_needed_to_extract);
  in->general_purpose_bit_flag
    = readLittleUHalfwordUnaligned(ex.general_purpose_bit_flag);
  in->last_mod_file_time
    = readLittleUHalfwordUnaligned(ex.last_mod_file_time);
  in->last_mod_file_date
    = readLittleUHalfwordUnaligned(ex.last_mod_file_date);
  in->crc32
    = readLittleUWordUnaligned(ex.crc32);
  in->compressed_size
    = readLittleUWordUnaligned(ex.compressed_size);
  in->uncompressed_size
    = readLittleUWordUnaligned(ex.uncompressed_size);
  in->filename_length
    = readLittleUHalfwordUnaligned(ex.filename_length);
  in->extra_field_length
    = readLittleUHalfwordUnaligned(ex.extra_field_length);
}

static void
import(Internal_CentralDirectoryHeader *in,
       const External_CentralDirectoryHeader &ex)
{
  memcpy(in->signature, ex.signature, 4);
  in->version_made_by
    = readLittleUHalfwordUnaligned(ex.version_made_by);
  in->version_needed_to_extract
    = readLittleUHalfwordUnaligned(ex.version_needed_to_extract);
  in->general_purpose_bit_flag
    = readLittleUHalfwordUnaligned(ex.general_purpose_bit_flag);
  in->compression_method
    = readLittleUHalfwordUnaligned(ex.compression_method);
  in->last_mod_file_time
    = readLittleUHalfwordUnaligned(ex.last_mod_file_time);
  in->last_mod_file_date
    = readLittleUHalfwordUnaligned(ex.last_mod_file_date);
  in->crc32
    = readLittleUWordUnaligned(ex.crc32);
  in->compressed_size
    = readLittleUWordUnaligned(ex.compressed_size);
  in->uncompressed_size
    = readLittleUWordUnaligned(ex.uncompressed_size);
  in->filename_length
    = readLittleUHalfwordUnaligned(ex.filename_length);
  in->extra_field_length
    = readLittleUHalfwordUnaligned(ex.extra_field_length);
  in->file_comment_length
    = readLittleUHalfwordUnaligned(ex.file_comment_length);
  in->disk_number_start
    = readLittleUHalfwordUnaligned(ex.disk_number_start);
  in->internal_file_attributes
    = readLittleUHalfwordUnaligned(ex.internal_file_attributes);
  in->external_file_attributes
    = readLittleUHalfwordUnaligned(ex.external_file_attributes);
  in->relative_offset_local_header
    = readLittleUWordUnaligned(ex.relative_offset_local_header);
}

static void
import(Internal_EndCentralDirectoryHeader *in,
       const External_EndCentralDirectoryHeader &ex)
{
  memcpy(in->signature, ex.signature, 4);
  in->number_this_disk
    = readLittleUHalfwordUnaligned(ex.number_this_disk);
  in->num_disk_with_start_central_dir
    = readLittleUHalfwordUnaligned(ex.num_disk_with_start_central_dir);
  in->num_entries_central_dir_this_disk
    = readLittleUHalfwordUnaligned(ex.num_entries_central_dir_this_disk);
  in->total_entries_central_dir
    = readLittleUHalfwordUnaligned(ex.total_entries_central_dir);
  in->size_central_directory
    = readLittleUWordUnaligned(ex.size_central_directory);
  in->offset_start_central_directory
    = readLittleUWordUnaligned(ex.offset_start_central_directory);
  in->zipfile_comment_length
    = readLittleUHalfwordUnaligned(ex.zipfile_comment_length);
}
