ifdef FOO
$(error FOO is not defined!)
endif

FOO = foo
FOOFOUND = false
BARFOUND = false
BAZFOUND = false

ifdef FOO
FOOFOUND = true
else ifdef BAR
BARFOUND = true
else
BAZFOUND = true
endif

BAR2 = bar2
FOO2FOUND = false
BAR2FOUND = false
BAZ2FOUND = false

ifdef FOO2
FOO2FOUND = true
else ifdef BAR2
BAR2FOUND = true
else
BAZ2FOUND = true
endif

FOO3FOUND = false
BAR3FOUND = false
BAZ3FOUND = false

ifdef FOO3
FOO3FOUND = true
else ifdef BAR3
BAR3FOUND = true
else
BAZ3FOUND = true
endif

ifdef RANDOM
CONTINUATION = \
else           \
endif
endif

ifndef ASDFJK
else
$(error ASFDJK was not set)
endif

TESTSET =

ifdef TESTSET
$(error TESTSET was not set)
endif

TESTEMPTY = $(NULL)
ifndef TESTEMPTY
$(error TEST-FAIL TESTEMPTY was probably expanded!)
endif

# ifneq ( a,a)
# $(error Arguments to ifeq should be stripped before evaluation)
# endif

XSPACE = x # trick

ifneq ($(NULL),$(NULL))
$(error TEST-FAIL ifneq)
endif

ifneq (x , x)
$(error argument-stripping1)
endif

ifeq ( x,x )
$(error argument-stripping2)
endif

ifneq ($(XSPACE), x )
$(error argument-stripping3)
endif

ifeq 'x ' ' x'
$(error TEST-FAIL argument-stripping4)
endif

all:
	test $(FOOFOUND) = true   # FOOFOUND
	test $(BARFOUND) = false  # BARFOUND
	test $(BAZFOUND) = false  # BAZFOUND
	test $(FOO2FOUND) = false # FOO2FOUND
	test $(BAR2FOUND) = true  # BAR2FOUND
	test $(BAZ2FOUND) = false # BAZ2FOUND
	test $(FOO3FOUND) = false # FOO3FOUND
	test $(BAR3FOUND) = false # BAR3FOUND
	test $(BAZ3FOUND) = true  # BAZ3FOUND
ifneq ($(FOO),foo)
	echo TEST-FAIL 'FOO neq foo: "$(FOO)"'
endif
ifneq ($(FOO), foo) # Whitespace after the comma is stripped
	echo TEST-FAIL 'FOO plus whitespace'
endif
ifeq ($(FOO), foo ) # But not trailing whitespace
	echo TEST-FAIL 'FOO plus trailing whitespace'
endif
ifeq ( $(FOO),foo) # Not whitespace after the paren
	echo TEST-FAIL 'FOO with leading whitespace'
endif
ifeq ($(FOO),$(NULL) foo) # Nor whitespace after expansion
	echo TEST-FAIL 'FOO with embedded ws'
endif
ifeq ($(BAR2),bar)
	echo TEST-FAIL 'BAR2 eq bar'
endif
ifeq '$(BAR3FOUND)' 'false'
	echo BAR3FOUND is ok
else
	echo TEST-FAIL BAR3FOUND is not ok
endif
ifndef FOO
	echo TEST-FAIL "foo not defined?"
endif
	@echo TEST-PASS
