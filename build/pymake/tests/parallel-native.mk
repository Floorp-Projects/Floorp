#T commandline: ['-j2']

# ensure that calling python commands doesn't block other targets
ifndef SLEEP
SLEEP := sleep
endif

PRINTF = printf "$@:0:" >>results
EXPECTED = target2:0:target1:0:

all:: target1 target2
	cat results
	test "$$(cat results)" = "$(EXPECTED)"
	@echo TEST-PASS

target1:
	$(SLEEP) 0.1
	$(PRINTF)

target2:
	$(PRINTF)
