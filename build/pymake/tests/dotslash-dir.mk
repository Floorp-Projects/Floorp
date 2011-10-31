#T grep-for: "dotslash-built"
.PHONY: $(dir foo)

all: $(dir foo)
	@echo TEST-PASS

$(dir foo):
	@echo dotslash-built
