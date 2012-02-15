# Process this file with automake to produce Makefile.in

NULL =

SUBDIRS = src util test

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = harfbuzz.pc

EXTRA_DIST = \
	autogen.sh \
	harfbuzz.doap \
	$(NULL)

MAINTAINERCLEANFILES = \
	$(srcdir)/INSTALL \
	$(srcdir)/aclocal.m4 \
	$(srcdir)/autoscan.log \
	$(srcdir)/compile \
	$(srcdir)/config.guess \
	$(srcdir)/config.h.in \
	$(srcdir)/config.sub \
	$(srcdir)/configure.scan \
	$(srcdir)/depcomp \
	$(srcdir)/install-sh \
	$(srcdir)/ltmain.sh \
	$(srcdir)/missing \
	$(srcdir)/mkinstalldirs \
	$(srcdir)/ChangeLog \
	`find "$(srcdir)" -type f -name Makefile.in -print`


#
# ChangeLog generation
#
CHANGELOG_RANGE =
ChangeLog: $(srcdir)/ChangeLog
$(srcdir)/ChangeLog:
	$(AM_V_GEN) if test -d "$(srcdir)/.git"; then \
	  (GIT_DIR=$(top_srcdir)/.git ./missing --run \
	   git log $(CHANGELOG_RANGE) --stat) | fmt --split-only > $@.tmp \
	  && mv -f $@.tmp $@ \
	  || ($(RM) $@.tmp; \
	      echo Failed to generate ChangeLog, your ChangeLog may be outdated >&2; \
	      (test -f $@ || echo git-log is required to generate this file >> $@)); \
	else \
	  test -f $@ || \
	  (echo A git checkout and git-log is required to generate ChangeLog >&2 && \
	  echo A git checkout and git-log is required to generate this file >> $@); \
	fi
.PHONY: $(srcdir)/ChangeLog


#
# Release engineering
#

# TODO: Copy infrastructure from cairo

TAR_OPTIONS = --owner=0 --group=0
dist-hook: dist-clear-sticky-bits
# Clean up any sticky bits we may inherit from parent dir
dist-clear-sticky-bits:
	chmod -R a-s $(distdir)


tar_file = $(PACKAGE_TARNAME)-$(VERSION).tar.bz2
sha256_file = $(tar_file).sha256
gpg_file = $(sha256_file).asc
$(sha256_file): $(tar_file)
	sha256sum $^ > $@
$(gpg_file): $(sha256_file)
	@echo "Please enter your GPG password to sign the checksum."
	gpg --armor --sign $^

release-files: $(tar_file) $(sha256_file) $(gpg_file)


-include $(top_srcdir)/git.mk
