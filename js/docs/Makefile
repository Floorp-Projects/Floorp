# directory to store APIDOC results.  This directory WILL BE DELETED before
# calling APIDOC!
OUTDIR    = gen/
PERL_BIN  = perl
PERL_ARGS =
CSSFILES  = api-content.css \
            api-toc.css

CLEAN_LS = ls --color=never

apidocs::
	rm -rf $(OUTDIR)
	mkdir $(OUTDIR)
	$(PERL_BIN) $(PERL_ARGS) parse_apidoc.pl jsref.xml $(OUTDIR)
	cp $(CSSFILES) $(OUTDIR)
	cd $(OUTDIR); $(CLEAN_LS) *.html > NOWRAP
