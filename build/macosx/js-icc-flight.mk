OBJDIR_ICC = $(MOZ_OBJDIR)/js-icc
OBJDIR_X86 = $(MOZ_OBJDIR)/i386

include $(OBJDIR_X86)/config/autoconf.mk
include $(TOPSRCDIR)/config/config.mk

# Just copy the icc-produced libmozjs.dylib over
postflight_all:
	echo "Postflight: copying libmozjs.dylib..."
	$(INSTALL) $(OBJDIR_ICC)/dist/bin/libmozjs.dylib $(OBJDIR_X86)/dist/$(MOZ_MACBUNDLE_NAME)/Contents/MacOS/
