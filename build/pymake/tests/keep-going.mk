#T commandline: ['-k']
#T returncode: 2
#T grep-for: "TEST-PASS"

all: t2 t3

t1:
	@false

t2: t1
	@echo TEST-FAIL

t3:
	@echo TEST-PASS
