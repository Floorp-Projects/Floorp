#T gmake skip
#T grep-for: "2f7cdd0b-7277-48c1-beaf-56cb0dbacb24"

ifdef __WIN32__
PS:=;
else
PS:=:
endif

export PATH := $(TESTPATH)/pathdir$(PS)$(PATH)

# Test two commands. The first one shouldn't go through the shell and the
# second one should. The pathdir subdirectory has a Windows executable called
# pathtest.exe and a shell script called pathtest. We don't care which one is
# run, just that one of the two is (we use a uuid + grep-for to make sure
# that happens).
#
# FAQ:
# Q. Why skip GNU Make?
# A. Because $(TESTPATH) is a Windows-style path, and MSYS make doesn't take
#    too kindly to Windows paths in the PATH environment variable.
#
# Q. Why use an exe and not a batch file?
# A. The use cases here were all exe files without the extension. Batch file
#    lookup has broken semantics if the .bat extension isn't passed.
#
# Q. Why are the commands silent?
# A. So that we don't pass the grep-for test by mistake.
all:
	@pathtest
	@pathtest | grep -q 2f7cdd0b-7277-48c1-beaf-56cb0dbacb24
	@echo TEST-PASS
