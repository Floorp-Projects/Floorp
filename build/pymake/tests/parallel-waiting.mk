#T commandline: ['-j2']

EXPECTED = target1:before:target2:1:target2:2:target2:3:target1:after

all:: target1 target2
	cat results
	test "$$(cat results)" = "$(EXPECTED)"
	@echo TEST-PASS

target1:
	printf "$@:before:" >>results
	sleep 4
	printf "$@:after" >>results

target2:
	sleep 0.2
	printf "$@:1:" >>results
	sleep 0.1
	printf "$@:2:" >>results
	sleep 0.1
	printf "$@:3:" >>results
