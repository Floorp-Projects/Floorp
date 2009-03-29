#T commandline: ['CVAR=c val=spac\\ed', 'OVAL=cline', 'OVAL2=cline2']

export EVAR = eval
override OVAL = makefile

# exporting an override variable doesn't mean it's an override variable
override OVAL2 = makefile2
export OVAL2

export ALLVAR
ALLVAR = general
all: ALLVAR = allspecific

all:
	test "$(MAKELEVEL)" = "0"
	$(MAKE) -f $(TESTPATH)/submake.makefile2
