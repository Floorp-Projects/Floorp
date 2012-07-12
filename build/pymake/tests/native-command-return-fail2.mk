#T gmake skip
#T returncode: 2

CMD = %pycmd asplode_return
PYCOMMANDPATH = $(TESTPATH) $(TESTPATH)/subdir

all:
	$(CMD) not-an-integer
