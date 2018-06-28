/*
 * Copyright © 2018  Ebrahim Byagowi
 *
 *  This is part of HarfBuzz, a text shaping library.
 *
 * Permission is hereby granted, without written agreement and without
 * license or royalty fees, to use, copy, modify, and distribute this
 * software and its documentation for any purpose, provided that the
 * above copyright notice and the following two paragraphs appear in
 * all copies of this software.
 *
 * IN NO EVENT SHALL THE COPYRIGHT HOLDER BE LIABLE TO ANY PARTY FOR
 * DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES
 * ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION, EVEN
 * IF THE COPYRIGHT HOLDER HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 *
 * THE COPYRIGHT HOLDER SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING,
 * BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS
 * ON AN "AS IS" BASIS, AND THE COPYRIGHT HOLDER HAS NO OBLIGATION TO
 * PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR MODIFICATIONS.
 */

#include "hb-static.cc"
#include <stdio.h>
#include <stdlib.h>
#include "hb-open-type-private.hh"

template <typename Type, int Bytes> struct LEInt;

template <typename Type>
struct LEInt<Type, 1>
{
  public:
  inline void set (Type V)
  {
    v = V;
  }
  inline operator Type (void) const
  {
    return v;
  }
  private: uint8_t v;
};
template <typename Type>
struct LEInt<Type, 2>
{
  public:
  inline void set (Type V)
  {
    v[1] = (V >>  8) & 0xFF;
    v[0] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[1] <<  8)
         + (v[0]      );
  }
  private: uint8_t v[2];
};
template <typename Type>
struct LEInt<Type, 3>
{
  public:
  inline void set (Type V)
  {
    v[2] = (V >> 16) & 0xFF;
    v[1] = (V >>  8) & 0xFF;
    v[0] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[2] << 16)
         + (v[1] <<  8)
         + (v[0]      );
  }
  private: uint8_t v[3];
};
template <typename Type>
struct LEInt<Type, 4>
{
  public:
  inline void set (Type V)
  {
    v[3] = (V >> 24) & 0xFF;
    v[2] = (V >> 16) & 0xFF;
    v[1] = (V >>  8) & 0xFF;
    v[0] = (V      ) & 0xFF;
  }
  inline operator Type (void) const
  {
    return (v[3] << 24)
         + (v[2] << 16)
         + (v[1] <<  8)
         + (v[0]      );
  }
  private: uint8_t v[4];
};

template <typename Type, unsigned int Size>
struct LEIntType
{
  inline void set (Type i) { v.set (i); }
  inline operator Type(void) const { return v; }
  inline bool sanitize (OT::hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this)));
  }
  protected:
  LEInt<Type, Size> v;
  public:
  DEFINE_SIZE_STATIC (Size);
};

typedef LEIntType<uint8_t,  1> LEUINT8;		/* 8-bit unsigned integer. */
typedef LEIntType<int8_t,   1> LEINT8;		/* 8-bit signed integer. */
typedef LEIntType<uint16_t, 2> LEUINT16;	/* 16-bit unsigned integer. */
typedef LEIntType<int16_t,  2> LEINT16;		/* 16-bit signed integer. */
typedef LEIntType<uint32_t, 4> LEUINT32;	/* 32-bit unsigned integer. */
typedef LEIntType<int32_t,  4> LEINT32;		/* 32-bit signed integer. */
typedef LEIntType<uint32_t, 3> LEUINT24;	/* 24-bit unsigned integer. */


struct LE_FONTINFO16
{
  inline bool sanitize (OT::hb_sanitize_context_t *c, unsigned int length) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) && c->check_range (this, length)));
  }

  // https://msdn.microsoft.com/en-us/library/cc194829.aspx
  enum charset_t
  {
    // dfCharSet possible values and the codepage they are indicating to
    ANSI	= 0x00,	// 1252
    DEFAULT	= 0x01,	//
    SYMBOL	= 0x02,	//
    SHIFTJIS	= 0x80,	// 932
    HANGUL	= 0x81,	// 949
    GB2312	= 0x86,	// 936
    CHINESEBIG5	= 0x88,	// 950
    GREEK	= 0xA1,	// 1253
    TURKISH	= 0xA2,	// 1254
    HEBREW	= 0xB1,	// 1255
    ARABIC	= 0xB2,	// 1256
    BALTIC	= 0xBA,	// 1257
    RUSSIAN	= 0xCC,	// 1251
    THAI	= 0xDE,	// 874
    EE		= 0xEE,	// 1250
    OEM		= 0xFF	//
  };

  inline const char* get_charset() const
  {
    switch (dfCharSet) {
    case ANSI: return "ISO8859";
    case DEFAULT: return "WinDefault";
    case SYMBOL: return "Symbol";
    case SHIFTJIS: return "JISX0208.1983";
    case HANGUL: return "MSHangul";
    case GB2312: return "GB2312.1980";
    case CHINESEBIG5: return "Big5";
    case GREEK: return "CP1253";
    case TURKISH: return "CP1254";
    case HEBREW: return "CP1255";
    case ARABIC: return "CP1256";
    case BALTIC: return "CP1257";
    case RUSSIAN: return "CP1251";
    case THAI: return "CP874";
    case EE: return "CP1250";
    case OEM: return "OEM";
    default: return "Unknown";
    }
  }

  inline unsigned int get_version () const
  {
    return dfVersion;
  }

  inline unsigned int get_weight () const
  {
    return dfWeight;
  }

  enum weight_t {
    DONTCARE	= 0,
    THIN	= 100,
    EXTRALIGHT	= 200,
    ULTRALIGHT	= 200,
    LIGHT	= 300,
    NORMAL	= 400,
    REGULAR	= 400,
    MEDIUM	= 500,
    SEMIBOLD	= 600,
    DEMIBOLD	= 600,
    BOLD	= 700,
    EXTRABOLD	= 800,
    ULTRABOLD	= 800,
    HEAVY	= 900,
    BLACK	= 900
  };

  inline void dump () const
  {
    // With https://github.com/juanitogan/mkwinfont/blob/master/python/dewinfont.py help
    // Should be implemented differently eventually, but for now
    unsigned int ctstart;
    unsigned int ctsize;
    if (dfVersion == 0x200)
    {
      ctstart = 0x76;
      ctsize = 4;
    }
    else
    {
      return; // must of ".fon"s are version 2 and even dewinfont V1 implmentation doesn't seem correct
      ctstart = 0x94;
      ctsize = 6;
    }
    // unsigned int maxwidth = 0;
    for (unsigned int i = dfFirstChar; i < dfLastChar; ++i)
    {
      unsigned int entry = ctstart + ctsize * (i-dfFirstChar);
      unsigned int w = (uint16_t) OT::StructAtOffset<LEUINT16> (this, entry);

      unsigned int off;
      if (ctsize == 4)
        off = (uint16_t) OT::StructAtOffset<LEUINT16> (this, entry+2);
      else
        off = (uint32_t) OT::StructAtOffset<LEUINT32> (this, entry+2);

      unsigned int widthbytes = (w + 7) / 8;
      for (unsigned int j = 0; j < dfPixHeight; ++j)
      {
        for (unsigned int k = 0; k < widthbytes; ++k)
	{
	  unsigned int bytepos = off + k * dfPixHeight + j;
	  const uint8_t b = (uint8_t) OT::StructAtOffset<LEINT8> (this, bytepos);
	  for (unsigned int a = 128; a > 0; a >>= 1)
	    printf (b & a ? "x" : ".");
	}
	printf ("\n");
      }
      printf ("\n\n");
    }
  }

  protected:
  LEUINT16	dfVersion;
  LEUINT32	dfSize;
  LEUINT8	dfCopyright[60];
  LEUINT16	dfType;
  LEUINT16	dfPoints;
  LEUINT16	dfVertRes;
  LEUINT16	dfHorizRes;
  LEUINT16	dfAscent;
  LEUINT16	dfInternalLeading;
  LEUINT16	dfExternalLeading;
  LEUINT8	dfItalic;
  LEUINT8	dfUnderline;
  LEUINT8	dfStrikeOut;
  LEUINT16	dfWeight; // see weight_t
  LEUINT8	dfCharSet;  // see charset_t
  LEUINT16	dfPixWidth;
  LEUINT16	dfPixHeight;
  LEUINT8	dfPitchAndFamily;
  LEUINT16	dfAvgWidth;
  LEUINT16	dfMaxWidth;
  LEUINT8	dfFirstChar;
  LEUINT8	dfLastChar;
  LEUINT8	dfDefaultChar;
  LEUINT8	dfBreakChar;
  LEUINT16	dfWidthBytes;
  LEUINT32	dfDevice;
  LEUINT32	dfFace;
  LEUINT32	dfBitsPointer;
  LEUINT32	dfBitsOffset;
  LEUINT8	dfReserved;
//   LEUINT32	dfFlags;
//   LEUINT16	dfAspace;
//   LEUINT16	dfBspace;
//   LEUINT16	dfCspace;
//   LEUINT32	dfColorPointer;
//   LEUINT32	dfReserved1[4];
  OT::UnsizedArrayOf<LEUINT8>
		dataZ;
  public:
  DEFINE_SIZE_ARRAY (118, dataZ);
};

struct NE_NAMEINFO
{
  friend struct NE_TYPEINFO;

  inline bool sanitize (OT::hb_sanitize_context_t *c, const void *base, unsigned int shift) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  get_font (base, shift).sanitize (c, length << shift)));
  }

  inline const LE_FONTINFO16& get_font (const void *base, int shift) const
  {
    return OT::StructAtOffset<LE_FONTINFO16> (base, offset << shift);
  }

  enum resource_type_flag_t {
    NONE     = 0x0000,
    MOVEABLE = 0x0010,
    PURE     = 0x0020,
    PRELOAD  = 0x0040
  };

  protected:
  LEUINT16	offset;	// Should be shifted with alignmentShiftCount before use
  LEUINT16	length;	// Should be shifted with alignmentShiftCount before use
  LEUINT16	flags;	// resource_type_flag_t
  LEUINT16	id;
  LEUINT16	handle;
  LEUINT16	usage;
  public:
  DEFINE_SIZE_STATIC (12);
};

struct NE_TYPEINFO
{
  inline bool sanitize (OT::hb_sanitize_context_t *c, const void *base, unsigned int shift) const
  {
    TRACE_SANITIZE (this);
    return_trace (c->check_struct (this) && resources.sanitize (c, count, base, shift));
  }

  inline unsigned int get_size (void) const
  { return 8 + count * NE_NAMEINFO::static_size; }

  inline const NE_TYPEINFO& next () const
  {
    const NE_TYPEINFO& next = OT::StructAfter<NE_TYPEINFO> (*this);
    if (type_id == 0)
      return Null(NE_TYPEINFO);
    return next;
  }

  inline const LE_FONTINFO16& get_font (unsigned int idx, const void *base, int shift) const
  {
    if (idx < count)
      return resources[idx].get_font (base, shift);
    return Null(LE_FONTINFO16);
  }

  inline unsigned int get_count () const
  {
    return count;
  }

  inline unsigned int get_type_id () const
  {
    return type_id;
  }

  enum type_id_t {
    CURSOR		= 0x8001,
    BITMAP		= 0x8002,
    ICON		= 0x8003,
    MENU		= 0x8004,
    DIALOG		= 0x8005,
    STRING		= 0x8006,
    FONT_DIRECTORY	= 0x8007,
    FONT		= 0x8008,
    ACCELERATOR_TABLE	= 0x8009,
    RESOURCE_DATA	= 0x800a,
    GROUP_CURSOR	= 0x800c,
    GROUP_ICON		= 0x800e,
    VERSION		= 0x8010
  };

  protected:
  LEUINT16	type_id; // see type_id_t
  LEUINT16	count;
  LEUINT32	resloader;
  OT::UnsizedArrayOf<NE_NAMEINFO>
		resources;
  public:
  DEFINE_SIZE_ARRAY (8, resources);
};

struct NE_RESOURCE_TABLE
{
  inline bool sanitize (OT::hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);

    if (!c->check_struct (this))
      return_trace (false);

    const NE_TYPEINFO* n = &chain;
    while (n != &Null(NE_TYPEINFO) && c->check_struct (n) && n->get_type_id () != 0)
    {
      if (n->get_type_id () == NE_TYPEINFO::FONT)
	return_trace (n->sanitize (c, base, alignmentShiftCount));
      n = &n->next();
    }
    return_trace (false);
  }

  inline unsigned int get_shift_value () const
  {
    return alignmentShiftCount;
  }

  inline const NE_TYPEINFO& get_fonts_entry () const
  {
    const NE_TYPEINFO* n = &chain;
    while (n != &Null(NE_TYPEINFO) && n->get_type_id () != 0)
    {
      if (n->get_type_id () == NE_TYPEINFO::FONT)
	return *n;
      n = &n->next();
    }
    return Null(NE_TYPEINFO);
  }

  protected:
  LEUINT16	alignmentShiftCount;
  NE_TYPEINFO	chain;
  // It is followed by an array of OT::ArrayOf<LEUINT8, LEUINT8> chars;
  public:
  DEFINE_SIZE_MIN (2);
};

// https://github.com/wine-mirror/wine/blob/master/include/winnt.h#L2467
struct LE_IMAGE_OS2_HEADER
{
  inline bool sanitize (OT::hb_sanitize_context_t *c, const void *base) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) && (this+rsrctab).sanitize (c, base)));
  }

  inline const NE_RESOURCE_TABLE& get_resource_table () const
  {
    if (magic != 0x454E) // Only NE containers are support for now, NE == 0x454E
      return Null(NE_RESOURCE_TABLE);
    return this+rsrctab;
  }

  protected:
  LEUINT16	magic;		/* 00 NE signature 'NE' */
  LEUINT8	ver;		/* 02 Linker version number */
  LEUINT8	rev;		/* 03 Linker revision number */
  LEUINT16	enttab;		/* 04 Offset to entry table relative to NE */
  LEUINT16	cbenttab;	/* 06 Length of entry table in bytes */
  LEUINT32	crc;		/* 08 Checksum */
  LEUINT16	flags;		/* 0c Flags about segments in this file */
  LEUINT16	autodata;	/* 0e Automatic data segment number */
  LEUINT16	heap;		/* 10 Initial size of local heap */
  LEUINT16	stack;		/* 12 Initial size of stack */
  LEUINT32	csip;		/* 14 Initial CS:IP */
  LEUINT32	sssp;		/* 18 Initial SS:SP */
  LEUINT16	cseg;		/* 1c # of entries in segment table */
  LEUINT16	cmod;		/* 1e # of entries in module reference tab. */
  LEUINT16	cbnrestab;	/* 20 Length of nonresident-name table     */
  LEUINT16	segtab;		/* 22 Offset to segment table */
  OT::OffsetTo<NE_RESOURCE_TABLE, LEUINT16>
		rsrctab;	/* 24 Offset to resource table */
  LEUINT16	restab;		/* 26 Offset to resident-name table */
  LEUINT16	modtab;		/* 28 Offset to module reference table */
  LEUINT16	imptab;		/* 2a Offset to imported name table */
  LEUINT32	nrestab;	/* 2c Offset to nonresident-name table */
  LEUINT16	cmovent;	/* 30 # of movable entry points */
  LEUINT16	align;		/* 32 Logical sector alignment shift count */
  LEUINT16	cres;		/* 34 # of resource segments */
  LEUINT8	exetyp;		/* 36 Flags indicating target OS */
  LEUINT8	flagsothers;	/* 37 Additional information flags */
  LEUINT16	pretthunks;	/* 38 Offset to return thunks */
  LEUINT16	psegrefbytes;	/* 3a Offset to segment ref. bytes */
  LEUINT16	swaparea;	/* 3c Reserved by Microsoft */
  LEUINT16	expver;		/* 3e Expected Windows version number */
  public:
  DEFINE_SIZE_STATIC (64);
};

struct LE_IMAGE_DOS_HEADER {
  inline bool sanitize (OT::hb_sanitize_context_t *c) const
  {
    TRACE_SANITIZE (this);
    return_trace (likely (c->check_struct (this) &&
			  get_os2_header ().sanitize (c, this)));
  }

  inline const LE_IMAGE_OS2_HEADER& get_os2_header () const
  {
    return this+e_lfanew;
  }

  protected:
  LEUINT16	e_magic;	// Magic number
  LEUINT16	e_cblp;		// Bytes on last page of file
  LEUINT16	e_cp;		// Pages in file
  LEUINT16	e_crlc;		// Relocations
  LEUINT16	e_cparhdr;	// Size of header in paragraphs
  LEUINT16	e_minalloc;	// Minimum extra paragraphs needed
  LEUINT16	e_maxalloc;	// Maximum extra paragraphs needed
  LEUINT16	e_ss;		// Initial (relative) SS value
  LEUINT16	e_sp;		// Initial SP value
  LEUINT16	e_csum;		// Checksum
  LEUINT16	e_ip;		// Initial IP value
  LEUINT16	e_cs;		// Initial (relative) CS value
  LEUINT16	e_lfarlc;	// File address of relocation table
  LEUINT16	e_ovno;		// Overlay number
  LEUINT16	e_res_0;	// Reserved words
  LEUINT16	e_res_1;	// Reserved words
  LEUINT16	e_res_2;	// Reserved words
  LEUINT16	e_res_3;	// Reserved words
  LEUINT16	e_oemid;	// OEM identifier (for e_oeminfo)
  LEUINT16	e_oeminfo;	// OEM information; e_oemid specific
  LEUINT16	e_res2_0;	// Reserved words
  LEUINT16	e_res2_1;	// Reserved words
  LEUINT16	e_res2_2;	// Reserved words
  LEUINT16	e_res2_3;	// Reserved words
  LEUINT16	e_res2_4;	// Reserved words
  LEUINT16	e_res2_5;	// Reserved words
  LEUINT16	e_res2_6;	// Reserved words
  LEUINT16	e_res2_7;	// Reserved words
  LEUINT16	e_res2_8;	// Reserved words
  LEUINT16	e_res2_9;	// Reserved words
  OT::OffsetTo<LE_IMAGE_OS2_HEADER, LEUINT32>
		e_lfanew;	// File address of new exe header
  public:
  DEFINE_SIZE_STATIC (64);
};

int main (int argc, char** argv) {
  hb_blob_t *blob = hb_blob_create_from_file (argv[1]);

  OT::Sanitizer<LE_IMAGE_DOS_HEADER> sanitizer;
  hb_blob_t *font_blob = sanitizer.sanitize (blob);
  const LE_IMAGE_DOS_HEADER* dos_header = font_blob->as<LE_IMAGE_DOS_HEADER> ();

  const NE_RESOURCE_TABLE &rtable = dos_header->get_os2_header ().get_resource_table ();
  int shift = rtable.get_shift_value ();
  const NE_TYPEINFO& entry = rtable.get_fonts_entry ();
  for (unsigned int i = 0; i < entry.get_count (); ++i)
  {
    const LE_FONTINFO16& font = entry.get_font (i, dos_header, shift);
    printf ("version: %x, weight: %d, charset: %s\n", font.get_version (),
	    font.get_weight (), font.get_charset ());
    // font.dump ();
  }
  return 0;
}
