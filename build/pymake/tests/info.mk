#T grep-for: "info-printed\ninfo-nth"
all:

INFO = info-printed

$(info $(INFO))
$(info $(subst second,nth,info-second))
$(info TEST-PASS)
