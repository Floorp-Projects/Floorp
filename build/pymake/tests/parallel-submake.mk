#T commandline: ['-j2']

# A submake shouldn't return control to the parent until it has actually finished doing everything.

all:
	-$(MAKE) -f $(TESTPATH)/parallel-submake.mk subtarget
	cat results
	test "$$(cat results)" = "0123"
	@echo TEST-PASS

subtarget: succeed-slowly fail-quickly

succeed-slowly:
	printf 0 >>results; sleep 1; printf 1 >>results; sleep 1; printf 2 >>results; sleep 1; printf 3 >>results

fail-quickly:
	exit 1
