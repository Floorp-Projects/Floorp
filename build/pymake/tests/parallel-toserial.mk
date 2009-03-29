#T commandline: ['-j4']

# Test that -j1 in a submake has the proper effect.

define SLOWCMD
printf "$@:0:" >>$(RFILE)
sleep 0.5
printf "$@:1:" >>$(RFILE)
endef

all: p1 p2
subtarget: s1 s2

p1 p2: RFILE = presult
s1 s2: RFILE = sresult

p1 s1:
	$(SLOWCMD)

p2 s2:
	sleep 0.1
	$(SLOWCMD)

all:
	$(MAKE) -j1 -f $(TESTPATH)/parallel-toserial.mk subtarget
	printf "presult: %s\n" "$$(cat presult)"
	test "$$(cat presult)" = "p1:0:p2:0:p1:1:p2:1:"
	printf "sresult: %s\n" "$$(cat sresult)"
	test "$$(cat sresult)" = "s1:0:s1:1:s2:0:s2:1:"
	@echo TEST-PASS

