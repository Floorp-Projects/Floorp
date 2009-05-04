#T commandline: ['-k', '-j2']
#T returncode: 2
#T grep-for: "TEST-PASS"

all: t1 slow1 slow2 slow3 t2

t2:
	@echo TEST-PASS

slow%:
	sleep 1
