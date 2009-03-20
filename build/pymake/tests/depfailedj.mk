#T returncode: 2
#T commandline: ['-j4']

$(shell touch foo.in)

all: foo.in foo.out missing
	@echo TEST-PASS

%.out: %.in
	cp $< $@
