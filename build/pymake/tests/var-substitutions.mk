SIMPLEVAR = aabb.cc
SIMPLEPERCENT = test_value%extra

SIMPLE3SUBSTNAME = SIMPLEVAR:.dd
$(SIMPLE3SUBSTNAME) = weirdval

PERCENT = dummy

SIMPLESUBST = $(SIMPLEVAR:.cc=.dd)
SIMPLE2SUBST = $(SIMPLEVAR:.cc)
SIMPLE3SUBST = $(SIMPLEVAR:.dd)
SIMPLE4SUBST = $(SIMPLEVAR:.cc=.dd=.ee)
SIMPLE5SUBST = $(SIMPLEVAR:.cc=%.dd)
PERCENTSUBST = $(SIMPLEVAR:%.cc=%.ee)
PERCENT2SUBST = $(SIMPLEVAR:aa%.cc=ff%.f)
PERCENT3SUBST = $(SIMPLEVAR:aa%.dd=gg%.gg)
PERCENT4SUBST = $(SIMPLEVAR:aa%.cc=gg)
PERCENT5SUBST = $(SIMPLEVAR:aa)
PERCENT6SUBST = $(SIMPLEVAR:%.cc=%.dd=%.ee)
PERCENT7SUBST = $(SIMPLEVAR:$(PERCENT).cc=%.dd)
PERCENT8SUBST = $(SIMPLEVAR:%.cc=$(PERCENT).dd)
PERCENT9SUBST = $(SIMPLEVAR:$(PERCENT).cc=$(PERCENT).dd)
PERCENT10SUBST = $(SIMPLEVAR:%%.bb.cc=zz.bb.cc)
PERCENT11SUBST = $(SIMPLEPERCENT:test%value%extra=other%value%extra)

SPACEDVAR = $(NULL)  ex1.c ex2.c $(NULL)
SPACEDSUBST = $(SPACEDVAR:.c=.o)

all:
	test "$(SIMPLESUBST)" = "aabb.dd"
	test "$(SIMPLE2SUBST)" = ""
	test "$(SIMPLE3SUBST)" = "weirdval"
	test "$(SIMPLE4SUBST)" = "aabb.dd=.ee"
	test "$(SIMPLE5SUBST)" = "aabb%.dd"
	test "$(PERCENTSUBST)" = "aabb.ee"
	test "$(PERCENT2SUBST)" = "ffbb.f"
	test "$(PERCENT3SUBST)" = "aabb.cc"
	test "$(PERCENT4SUBST)" = "gg"
	test "$(PERCENT5SUBST)" = ""
	test "$(PERCENT6SUBST)" = "aabb.dd=%.ee"
	test "$(PERCENT7SUBST)" = "aabb.dd"
	test "$(PERCENT8SUBST)" = "aabb.dd"
	test "$(PERCENT9SUBST)" = "aabb.dd"
	test "$(PERCENT10SUBST)" = "aabb.cc"
	test "$(PERCENT11SUBST)" = "other_value%extra"
	test "$(SPACEDSUBST)" = "ex1.o ex2.o"
	@echo TEST-PASS

PERCENT = %
