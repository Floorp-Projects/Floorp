#T gmake skip

CMD = %pycmd asplode
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD) 0
	-$(CMD) 1
	$(CMD) None
	-$(CMD) not-an-integer
	@echo TEST-PASS
