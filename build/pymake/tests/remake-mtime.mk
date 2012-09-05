# mtime(dep1) < mtime(target) so the target should not be made
$(shell touch dep1; sleep 1; touch target)

all: target
	echo TEST-PASS

target: dep1
	echo TEST-FAIL target should not have been made

dep1: dep2
	@echo "Remaking dep1 (actually not)"

dep2:
	@echo "Making dep2 (actually not)"
