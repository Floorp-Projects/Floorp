codegen_cpu_cxxsrc = $(error Unrecognized target CPU.)

ifeq (i686,$(TARGET_CPU))
codegen_cpu_cxxsrc := Ia32Assembler.cpp
endif

ifeq (powerpc,$(TARGET_CPU))
codegen_cpu_cxxsrc := PpcAssembler.cpp
endif

ifeq (arm,$(TARGET_CPU))
codegen_cpu_cxxsrc := ArmAssembler.cpp
endif

avmplus_CXXSRCS := $(avmplus_CXXSRCS) \
  $(curdir)/CodegenMIR.cpp \
  $(curdir)/$(codegen_cpu_cxxsrc) \
  $(NULL)
