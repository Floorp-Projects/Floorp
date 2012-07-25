# On Windows, MSYS make takes Unix paths but Pymake takes Windows paths
VPSEP := $(if $(and $(__WIN32__),$(.PYMAKE)),;,:)

$(shell \
mkdir subd1 subd2 subd3; \
printf "reallybaddata" >subd1/foo.in; \
printf "gooddata" >subd2/foo.in; \
printf "baddata" >subd3/foo.in; \
touch subd1/foo.in2 subd2/foo.in2 subd3/foo.in2; \
)

vpath %.in subd

vpath
vpath %.in subd2$(VPSEP)subd3

vpath %.in2 subd0
vpath f%.in2 subd1
vpath %.in2 $(VPSEP)subd2

%.out: %.in
	test "$<" = "subd2/foo.in"
	cp $< $@

%.out2: %.in2
	test "$<" = "subd1/foo.in2"
	cp $< $@

all: foo.out foo.out2
	test "$$(cat foo.out)" = "gooddata"
	@echo TEST-PASS
