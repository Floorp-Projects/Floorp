all:
	@  sh -c 'if [ $$# = 3 ] ; then echo TEST-PASS; else echo TEST-FAIL; fi' -- a "" b  
