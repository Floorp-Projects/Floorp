#T commandline: ['-j2']

# CAUTION: this makefile is also used by serial-toparallel.mk

define SLOWMAKE
printf "$@:0:" >>results
sleep 0.5
printf "$@:1:" >>results
sleep 0.5
printf "$@:2:" >>results
endef

EXPECTED = target1:0:target2:0:target1:1:target2:1:target1:2:target2:2:

all:: target1 target2
	cat results
	test "$$(cat results)" = "$(EXPECTED)"
	@echo TEST-PASS

target1:
	$(SLOWMAKE)

target2:
	sleep 0.1
	$(SLOWMAKE)

.PHONY: all
