/* jconfig.h.  Generated automatically by configure.  */
/* jconfig.auto --- source file edited by configure script */
/* see jconfig.doc for explanations */

#ifdef __MWERKS__ /* macintosh */
      #error should be including jconfig-mac-cw.h!
#endif

#ifdef _WINDOWS /* windows */
      #error should be including jwinfig.h!
#endif

#ifdef XP_OS2 /* os2 */
      #error should be including jos2fig.h!
#endif

#define HAVE_PROTOTYPES 
#define HAVE_UNSIGNED_CHAR 
#define HAVE_UNSIGNED_SHORT 
#undef void
#undef const
#define CHAR_IS_UNSIGNED 
#define HAVE_STDDEF_H 
#define HAVE_STDLIB_H 
#undef NEED_BSD_STRINGS
#undef NEED_SYS_TYPES_H
#undef NEED_FAR_POINTERS
#undef NEED_SHORT_EXTERNAL_NAMES
/* Define this if you get warnings about undefined structures. */
#undef INCOMPLETE_TYPES_BROKEN

#ifdef JPEG_INTERNALS

#undef RIGHT_SHIFT_IS_UNSIGNED

#define INLINE __inline
/* These are for configuring the JPEG memory manager. */
#undef DEFAULT_MAX_MEM
#undef NO_MKTEMP

#endif /* JPEG_INTERNALS */

#ifdef JPEG_CJPEG_DJPEG

#define BMP_SUPPORTED		/* BMP image file format */
#define GIF_SUPPORTED		/* GIF image file format */
#define PPM_SUPPORTED		/* PBMPLUS PPM/PGM image file format */
#undef RLE_SUPPORTED		/* Utah RLE image file format */
#define TARGA_SUPPORTED		/* Targa image file format */

#undef TWO_FILE_COMMANDLINE
#undef NEED_SIGNAL_CATCHER
#undef DONT_USE_B_MODE

/* Define this if you want percent-done progress reports from cjpeg/djpeg. */
#undef PROGRESS_REPORT

#endif /* JPEG_CJPEG_DJPEG */
