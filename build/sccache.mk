preflight:
	# Terminate any sccache server that might still be around
	-python2.7 $(TOPSRCDIR)/sccache/sccache.py > /dev/null 2>&1

postflight:
	# Terminate sccache server. This prints sccache stats.
	-python2.7 $(TOPSRCDIR)/sccache/sccache.py
