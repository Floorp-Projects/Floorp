WFLAGS = -Wmissing-prototypes -Wstrict-prototypes -Wunused \
         -Wswitch


if DEBUG
CXXFLAGS = -DXP_UNIX -g -DDEBUG -DNEW_PARSER $(WFLAGS)
else
CXXFLAGS = -DXP_UNIX -O2 -DNEW_PARSER -Wuninitialized $(WFLAGS)
endif
