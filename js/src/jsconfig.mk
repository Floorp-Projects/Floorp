ifndef OBJDIR
  ifdef OBJDIR_NAME
    OBJDIR = $(OBJDIR_NAME)
  endif
endif

NSPR_VERSION     = 19980120
NSPR_LOCAL       = $(MOZ_DEPTH)/dist/$(OBJDIR)/nspr
NSPR_DIST        = $(MOZ_DEPTH)/dist/$(OBJDIR)
NSPR_OBJDIR      = $(OBJDIR)
ifeq ($(OS_ARCH), SunOS)
  NSPR_OBJDIR   := $(subst _sparc,,$(NSPR_OBJDIR))
endif
ifeq ($(OS_ARCH), Linux)
  NSPR_OBJDIR   := $(subst _All,ELF2.0,$(NSPR_OBJDIR))
endif
ifeq ($(OS_ARCH), AIX)
  NSPR_OBJDIR   := $(subst 4.1,4.2,$(NSPR_OBJDIR))
endif
ifeq ($(OS_ARCH), WINNT)
  NSPR_OBJDIR   := $(subst WINNT,WIN95,$(NSPR_OBJDIR))
  ifeq ($(OBJDIR), WIN32_D.OBJ)
    NSPR_OBJDIR  = WIN954.0_DBG.OBJ
  endif
  ifeq ($(OBJDIR), WIN32_O.OBJ)
    NSPR_OBJDIR  = WIN954.0_OPT.OBJ
  endif
endif
NSPR_SHARED      = /share/builds/components/nspr20/$(NSPR_VERSION)/$(NSPR_OBJDIR)
ifeq ($(OS_ARCH), WINNT)
  NSPR_SHARED    = nspr20/$(NSPR_VERSION)/$(NSPR_OBJDIR)
endif
NSPR_VERSIONFILE = $(NSPR_LOCAL)/Version
NSPR_CURVERSION := $(shell cat $(NSPR_VERSIONFILE))

get_nspr:
	@echo "Grabbing NSPR component..."
ifeq ($(NSPR_VERSION), $(NSPR_CURVERSION))
	@echo "No need, NSPR is up to date in this tree (ver=$(NSPR_VERSION))."
else
	mkdir -p $(NSPR_LOCAL)
	mkdir -p $(NSPR_DIST)
  ifneq ($(OS_ARCH), WINNT)
	cp       $(NSPR_SHARED)/*.jar $(NSPR_LOCAL)
  else
	sh       $(MOZ_DEPTH)/../reltools/compftp.sh $(NSPR_SHARED) $(NSPR_LOCAL) *.jar
  endif
	unzip -o $(NSPR_LOCAL)/mdbinary.jar -d $(NSPR_DIST)
	mkdir -p $(NSPR_DIST)/include
	unzip -o $(NSPR_LOCAL)/mdheader.jar -d $(NSPR_DIST)/include
	rm -rf   $(NSPR_DIST)/META-INF
	rm -rf   $(NSPR_DIST)/include/META-INF
	echo $(NSPR_VERSION) > $(NSPR_VERSIONFILE)
endif

SHIP_DIST  = $(MOZ_DEPTH)/dist/$(OBJDIR)
SHIP_DIR   = $(SHIP_DIST)/SHIP

SHIP_LIBS  = jsj.so libjs.so libnspr21.so
ifeq ($(OS_ARCH), WINNT)
 SHIP_LIBS = jsj.dll js32.dll libnspr21.dll
endif
ifeq ($(OS_ARCH), HP-UX)
 SHIP_LIBS = jsj.so libjs.so libnspr21.sl
endif
SHIP_LIBS := $(addprefix $(SHIP_DIST)/lib/, $(SHIP_LIBS))

SHIP_INCS  = js*.h netscape*.h
SHIP_INCS := $(addprefix $(SHIP_DIST)/include/, $(SHIP_INCS))

SHIP_BINS  = js jsj
ifeq ($(OS_ARCH), WINNT)
 SHIP_BINS = js.exe jsj.exe
endif
SHIP_BINS := $(addprefix $(SHIP_DIST)/bin/, $(SHIP_BINS))

ifdef BUILD_OPT
  JSREFJAR = jsref_opt.jar
else
  JSREFJAR = jsref_dbg.jar
endif

ship:
	mkdir -p $(SHIP_DIR)/lib
	mkdir -p $(SHIP_DIR)/include
	mkdir -p $(SHIP_DIR)/bin
	cp $(SHIP_LIBS) $(SHIP_DIR)/lib
	cp $(SHIP_INCS) $(SHIP_DIR)/include
	cp $(SHIP_BINS) $(SHIP_DIR)/bin
	cd $(SHIP_DIR); \
	  zip -r $(JSREFJAR) bin lib include
ifdef BUILD_SHIP
	cp $(SHIP_DIR)/$(JSREFJAR) $(BUILD_SHIP)
endif
