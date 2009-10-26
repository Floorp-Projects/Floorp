# avoid infinite loops by not remaking makefiles with
# double-colon no-dependency rules
# http://www.gnu.org/software/make/manual/make.html#Remaking-Makefiles
-include notfound.mk

all:
	@echo TEST-PASS

notfound.mk::
	@echo TEST-FAIL
