#T gmake skip

CMD = %pycmd asplode
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD) 0
	@echo TEST-PASS
