# directory to store APIDOC results.  This directory WILL BE DELETED before
# calling APIDOC!
OUTDIR = gen/

CSSFILES = api-content.css \
           api-toc.css

CLEAN_LS = ls --color=never

apidocs::
	rm -rf $(OUTDIR)
	mkdir $(OUTDIR)
	perl parse_apidoc.pl jsref.xml $(OUTDIR)
	cp $(CSSFILES) $(OUTDIR)
	cd $(OUTDIR); $(CLEAN_LS) *.html > NOWRAP
