cairoincludedir = $(includedir)/cairo
cairoinclude_HEADERS =	\
	cairo.h		\
	cairo-atsui.h	\
	cairo-features.h\
	cairo-ft.h	\
	cairo-glitz.h	\
	cairo-pdf.h	\
	cairo-png.h	\
	cairo-ps.h	\
	cairo-quartz.h	\
	cairo-xcb.h	\
	cairo-xlib.h

lib_LTLIBRARIES = libcairo.la

if CAIRO_HAS_PS_SURFACE
libcairo_ps_sources = cairo_ps_surface.c cairo-ps.h
endif

if CAIRO_HAS_PDF_SURFACE
libcairo_pdf_sources = cairo_pdf_surface.c cairo-pdf.h
endif

if CAIRO_HAS_PNG_SURFACE
libcairo_png_sources = cairo_png_surface.c cairo-png.h
endif

if CAIRO_HAS_XLIB_SURFACE
libcairo_xlib_sources = cairo_xlib_surface.c cairo-xlib.h
endif

if CAIRO_HAS_QUARTZ_SURFACE
libcairo_quartz_sources = cairo_quartz_surface.c cairo-quartz.h
endif

if CAIRO_HAS_XCB_SURFACE
libcairo_xcb_sources = cairo_xcb_surface.c cairo-xcb.h
endif

if CAIRO_HAS_GLITZ_SURFACE
libcairo_glitz_sources = cairo_glitz_surface.c cairo-glitz.h
endif

if CAIRO_HAS_ATSUI_FONT
libcairo_atsui_sources = cairo_atsui_font.c cairo-atsui.h
endif

if CAIRO_HAS_FT_FONT
libcairo_ft_sources = cairo_ft_font.c cairo-ft.h
endif

# These names match automake style variable definition conventions so
# without these lines, automake will complain during the handling of 
# the libcairo_la_LIBADD below.  (The INCLUDES is an autoconf only
# term and automake does not care about it)
FONTCONFIG_LIBS=@FONTCONFIG_LIBS@
XRENDER_LIBS=@XRENDER_LIBS@

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
	cairoint.h

libcairo_la_LDFLAGS = -version-info @VERSION_INFO@ -no-undefined

INCLUDES = -I$(srcdir) $(CAIRO_CFLAGS)

libcairo_la_LIBADD = $(CAIRO_LIBS)
