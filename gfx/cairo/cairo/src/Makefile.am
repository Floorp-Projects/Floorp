
if CAIRO_HAS_PS_SURFACE
libcairo_ps_headers = cairo-ps.h
libcairo_ps_sources = cairo_ps_surface.c
endif

if CAIRO_HAS_PDF_SURFACE
libcairo_pdf_headers = cairo-pdf.h
libcairo_pdf_sources = cairo_pdf_surface.c
endif

if CAIRO_HAS_PNG_SURFACE
libcairo_png_headers = cairo-png.h
libcairo_png_sources = cairo_png_surface.c
endif

if CAIRO_HAS_XLIB_SURFACE
libcairo_xlib_headers = cairo-xlib.h
libcairo_xlib_sources = cairo_xlib_surface.c
endif

if CAIRO_HAS_QUARTZ_SURFACE
libcairo_quartz_headers = cairo-quartz.h
libcairo_quartz_sources = cairo_quartz_surface.c
endif

if CAIRO_HAS_XCB_SURFACE
libcairo_xcb_headers = cairo-xcb.h
libcairo_xcb_sources = cairo_xcb_surface.c
endif

libcairo_win32_sources =
if CAIRO_HAS_WIN32_SURFACE
libcairo_win32_headers = cairo-win32.h
libcairo_win32_sources += cairo_win32_surface.c cairo-win32-private.h
endif
if CAIRO_HAS_WIN32_FONT
libcairo_win32_sources += cairo_win32_font.c
endif

if CAIRO_HAS_GLITZ_SURFACE
libcairo_glitz_headers = cairo-glitz.h
libcairo_glitz_sources = cairo_glitz_surface.c
endif

if CAIRO_HAS_ATSUI_FONT
libcairo_atsui_headers = cairo-atsui.h
libcairo_atsui_sources = cairo_atsui_font.c
endif

if CAIRO_HAS_FT_FONT
libcairo_ft_headers = cairo-ft.h
libcairo_ft_sources = cairo_ft_font.c cairo-ft-private.h
endif

# These names match automake style variable definition conventions so
# without these lines, automake will complain during the handling of 
# the libcairo_la_LIBADD below.  (The INCLUDES is an autoconf only
# term and automake does not care about it)
FONTCONFIG_LIBS=@FONTCONFIG_LIBS@
XRENDER_LIBS=@XRENDER_LIBS@

cairoincludedir = $(includedir)/cairo
cairoinclude_HEADERS =			\
	cairo.h				\
	cairo-features.h		\
	$(libcairo_atsui_headers)	\
	$(libcairo_ft_headers)		\
	$(libcairo_glitz_headers)	\
	$(libcairo_pdf_headers)		\
	$(libcairo_png_headers)		\
	$(libcairo_ps_headers)		\
	$(libcairo_quartz_headers)	\
	$(libcairo_win32_headers)	\
	$(libcairo_xcb_headers)		\
	$(libcairo_xlib_headers)

lib_LTLIBRARIES = libcairo.la

libcairo_la_SOURCES = 		\
	cairo.c			\
	cairo.h			\
	cairo_array.c		\
	cairo_cache.c		\
	cairo_color.c		\
	cairo_fixed.c		\
	cairo_font.c		\
	cairo_gstate.c		\
	cairo_hull.c		\
	cairo_image_surface.c	\
	cairo_matrix.c		\
	cairo_path.c		\
	cairo_path_bounds.c	\
	cairo_path_fill.c	\
	cairo_path_stroke.c	\
	cairo_pen.c		\
	cairo_polygon.c		\
	cairo_slope.c		\
	cairo_spline.c		\
	cairo_surface.c		\
	cairo_traps.c		\
	cairo_pattern.c		\
	cairo_unicode.c		\
	cairo_wideint.c		\
	cairo-wideint.h		\
	$(libcairo_atsui_sources)\
	$(libcairo_ft_sources)\
	$(libcairo_ps_sources)  \
	$(libcairo_pdf_sources) \
	$(libcairo_png_sources) \
	$(libcairo_xlib_sources)\
	$(libcairo_quartz_sources)\
	$(libcairo_xcb_sources) \
	$(libcairo_glitz_sources)\
	$(libcairo_win32_sources)\
	$(libcairo_freetype_sources)  \
	cairoint.h

libcairo_la_LDFLAGS = -version-info @VERSION_INFO@ -no-undefined

INCLUDES = -I$(srcdir) $(CAIRO_CFLAGS)

libcairo_la_LIBADD = $(CAIRO_LIBS)

install-data-local:
	@if test -f $(includedir)/cairo.h || test -f $(includedir)/cairo-features.h ; then \
	  echo "****************************************************************" ;	\
	  echo "*** Error: Old headers found. You should remove the following" ;	\
	  echo "***        files and then type 'make install' again." ;			\
	  ls $(includedir)/cairo*.h ;							\
	  echo "****************************************************************" ;	\
	  false ;									\
        fi
