SUBDIRS = src

EXTRA_DIST = \
	COPYING \
	COPYING-LGPL-2.1 \
	COPYING-MPL-1.1 \
	cairo.pc.in

pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = cairo.pc

# Some custom targets to make it easier to release things.
# Use either:
#		make release-check
# or		make release-publish

RELEASE_UPLOAD_HOST =   cairographics.org
RELEASE_UPLOAD_DIR =    /srv/cairo.freedesktop.org/www/snapshots
RELEASE_URL_BASE = 	http://cairographics.org/snapshots
RELEASE_ANNOUNCE_LIST = cairo-announce@cairographics.org

tar_file = $(PACKAGE)-$(VERSION).tar.gz
md5_file = $(tar_file).md5

$(md5_file): $(tar_file)
	md5sum $^ > $@

release-remove-old:
	rm -f $(tar_file) $(md5_file)

release-check: release-remove-old distcheck $(md5_file)

release-verify-newer:
	@echo -n "Checking that no $(VERSION) release already exists..."
	@ssh $(RELEASE_UPLOAD_HOST) test ! -e $(RELEASE_UPLOAD_DIR)/$(tar_file) \
		|| (echo "Ouch." && echo "Found: $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)/$(tar_file)" \
		&& echo "The version in configure.in must be incremented before a new release." \
		&& false)
	@echo "Good."

release-publish: release-verify-newer release-check
	mkdir -p releases
	scp $(tar_file) $(md5_file) $(RELEASE_UPLOAD_HOST):$(RELEASE_UPLOAD_DIR)
	mv $(tar_file) $(md5_file) releases
	ssh $(RELEASE_UPLOAD_HOST) "rm -f $(RELEASE_UPLOAD_DIR)/LATEST-$(PACKAGE)-[0-9]* && touch $(RELEASE_UPLOAD_DIR)/LATEST-$(PACKAGE)-$(VERSION)"
	@echo ""
	@echo "Please send an announcement to $(RELEASE_ANNOUNCE_LIST)"
	@echo "including the following:"
	@echo ""
	@echo "Subject: $(PACKAGE) snapshot $(VERSION) now available"
	@echo ""
	@echo "A new $(PACKAGE) snapshot $(VERSION) is now available from:"
	@echo ""
	@echo "	$(RELEASE_URL_BASE)/$(tar_file)"
	@echo "	$(RELEASE_URL_BASE)/$(md5_file)"
	@echo -n "	"
	@cat releases/$(md5_file)
	@echo ""
	@echo "Also, please include the new entries from the NEWS file."
