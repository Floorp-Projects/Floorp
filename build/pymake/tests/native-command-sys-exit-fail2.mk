#T gmake skip
#T returncode: 2

CMD = %pycmd asplode
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD) not-an-integer
