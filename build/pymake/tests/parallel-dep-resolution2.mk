#T commandline: ['-j3']
#T returncode: 2

all::
	sleep 1
	touch somefile

all:: somefile
	@echo TEST-PASS
