#
# God of Unix builds.
#

#
# Static macros.  Can be overridden on the command-line.
#
ifndef CVSROOT
CVSROOT		= /m/src
endif

# Allow for cvs flags
ifndef CVS_FLAGS
CVS_FLAGS = 
endif

CVS_CMD		= cvs $(CVS_FLAGS)
TARGETS		= export libs install
LDAP_MODULE      = LDAPCClientLibrary


#
# Environment variables.
#
ifdef MOZ_DATE
CVS_BRANCH      = -D "$(MOZ_DATE)"
endif

ifdef MOZ_BRANCH
CVS_BRANCH      = -r $(MOZ_BRANCH)
endif


ifndef MOZ_LDAPVER
MOZ_LDAPVER = -r Ldapsdk31_StableBuild
endif


all: setup build

.PHONY: all setup build

setup:
ifdef LDAP_MODULE
	$(CVS_CMD) -q co $(MOZ_LDAPVER) $(LDAP_MODULE)
endif

build:
	cd directory; $(MAKE) $(TARGETS)
