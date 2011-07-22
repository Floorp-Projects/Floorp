## $(TOUCH) and $(RM) are native commands in pymake.
## Test that pymake --just-print just prints them.

ifndef TOUCH
TOUCH = touch
endif

all: 
	$(RM) justprint-native-file1.txt
	$(TOUCH) justprint-native-file2.txt
	$(MAKE) --just-print -f $(TESTPATH)/justprint-native.mk justprint_target > justprint.log
#       make --just-print shouldn't have actually done anything.
	test ! -f justprint-native-file1.txt
	test -f justprint-native-file2.txt
#	but it should have printed each command
	grep -q 'touch justprint-native-file1.txt' justprint.log
	grep -q 'rm -f justprint-native-file2.txt' justprint.log
	grep -q 'this string is "unlikely to appear in the log by chance"' justprint.log
#	tidy up
	$(RM) justprint-native-file2.txt
	@echo TEST-PASS

justprint_target:
	$(TOUCH) justprint-native-file1.txt
	$(RM) justprint-native-file2.txt
	this string is "unlikely to appear in the log by chance"

.PHONY: justprint_target
