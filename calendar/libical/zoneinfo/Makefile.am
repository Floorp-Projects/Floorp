
zoneinfodatadir = $(datadir)/$(PACKAGE)/zoneinfo

zoneinfodata_DATA = zones.tab

DIRS = \
	Africa			\
	America			\
	America/Indiana		\
	America/Kentucky	\
	Antarctica		\
	Arctic			\
	Asia			\
	Atlantic		\
	Australia		\
	Europe			\
	Indian			\
	Pacific


install-data-local:
	@$(NORMAL_INSTALL)
	for dir in $(DIRS); do \
	  $(mkinstalldirs) $(DESTDIR)$(datadir)/$(PACKAGE)/zoneinfo/$$dir; \
	  for tzfile in $(srcdir)/$$dir/*.ics; do \
	    $(INSTALL_DATA) $$tzfile $(DESTDIR)$(datadir)/$(PACKAGE)/zoneinfo/$$dir; \
	  done \
	done

dist-hook:
	for dir in $(DIRS); do \
	  mkdir $(distdir)/$$dir; \
	  for tzfile in $(srcdir)/$$dir/*.ics; do \
	    cp -p $$tzfile $(distdir)/$$dir; \
	  done \
	done

EXTRA_DIST = $(zoneinfodata_DATA)
