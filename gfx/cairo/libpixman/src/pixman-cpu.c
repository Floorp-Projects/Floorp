/*
 * Copyright © 2000 SuSE, Inc.
 * Copyright © 2007 Red Hat, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that
 * copyright notice and this permission notice appear in supporting
 * documentation, and that the name of SuSE not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  SuSE makes no representations about the
 * suitability of this software for any purpose.  It is provided "as is"
 * without express or implied warranty.
 *
 * SuSE DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT SHALL SuSE
 * BE LIABLE FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION
 * OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <string.h>

#if defined(USE_ARM_SIMD) && defined(_MSC_VER)
/* Needed for EXCEPTION_ILLEGAL_INSTRUCTION */
#include <windows.h>
#endif

#include "pixman-private.h"

#ifdef USE_VMX

/* The CPU detection code needs to be in a file not compiled with
 * "-maltivec -mabi=altivec", as gcc would try to save vector register
 * across function calls causing SIGILL on cpus without Altivec/vmx.
 */
static pixman_bool_t initialized = FALSE;
static volatile pixman_bool_t have_vmx = TRUE;

#ifdef __APPLE__
#include <sys/sysctl.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    if (!initialized)
    {
	size_t length = sizeof(have_vmx);
	int error =
	    sysctlbyname ("hw.optional.altivec", &have_vmx, &length, NULL, 0);

	if (error)
	    have_vmx = FALSE;

	initialized = TRUE;
    }
    return have_vmx;
}

#elif defined (__linux__)
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <linux/auxvec.h>
#include <asm/cputable.h>

static pixman_bool_t
pixman_have_vmx (void)
{
    if (!initialized)
    {
	char fname[64];
	unsigned long buf[64];
	ssize_t count = 0;
	pid_t pid;
	int fd, i;

	pid = getpid ();
	snprintf (fname, sizeof(fname) - 1, "/proc/%d/auxv", pid);

	fd = open (fname, O_RDONLY);
	if (fd >= 0)
	{
	    for (i = 0; i <= (count / sizeof(unsigned long)); i += 2)
	    {
		/* Read more if buf is empty... */
		if (i == (count / sizeof(unsigned long)))
		{
		    count = read (fd, buf, sizeof(buf));
		    if (count <= 0)
			break;
		    i = 0;
		}

		if (buf[i] == AT_HWCAP)
		{
		    have_vmx = !!(buf[i + 1] & PPC_FEATURE_HAS_ALTIVEC);
		    initialized = TRUE;
		    break;
		}
		else if (buf[i] == AT_NULL)
		{
		    break;
		}
	    }
	    close (fd);
	}
    }
    if (!initialized)
    {
	/* Something went wrong. Assume 'no' rather than playing
	   fragile tricks with catching SIGILL. */
	have_vmx = FALSE;
	initialized = TRUE;
    }

    return have_vmx;
}

#else /* !__APPLE__ && !__linux__ */
#include <signal.h>
#include <setjmp.h>

static jmp_buf jump_env;

static void
vmx_test (int        sig,
	  siginfo_t *si,
	  void *     unused)
{
    longjmp (jump_env, 1);
}

static pixman_bool_t
pixman_have_vmx (void)
{
    struct sigaction sa, osa;
    int jmp_result;

    if (!initialized)
    {
	sa.sa_flags = SA_SIGINFO;
	sigemptyset (&sa.sa_mask);
	sa.sa_sigaction = vmx_test;
	sigaction (SIGILL, &sa, &osa);
	jmp_result = setjmp (jump_env);
	if (jmp_result == 0)
	{
	    asm volatile ( "vor 0, 0, 0" );
	}
	sigaction (SIGILL, &osa, NULL);
	have_vmx = (jmp_result == 0);
	initialized = TRUE;
    }
    return have_vmx;
}

#endif /* __APPLE__ */
#endif /* USE_VMX */

#if defined(USE_ARM_SIMD) || defined(USE_ARM_NEON)

#if defined(_MSC_VER)

#if defined(USE_ARM_SIMD)
extern int pixman_msvc_try_arm_simd_op ();

pixman_bool_t
pixman_have_arm_simd (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t have_arm_simd = FALSE;

    if (!initialized)
    {
	__try {
	    pixman_msvc_try_arm_simd_op ();
	    have_arm_simd = TRUE;
	} __except (GetExceptionCode () == EXCEPTION_ILLEGAL_INSTRUCTION) {
	    have_arm_simd = FALSE;
	}
	initialized = TRUE;
    }

    return have_arm_simd;
}

#endif /* USE_ARM_SIMD */

#if defined(USE_ARM_NEON)
extern int pixman_msvc_try_arm_neon_op ();

pixman_bool_t
pixman_have_arm_neon (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t have_arm_neon = FALSE;

    if (!initialized)
    {
	__try
	{
	    pixman_msvc_try_arm_neon_op ();
	    have_arm_neon = TRUE;
	}
	__except (GetExceptionCode () == EXCEPTION_ILLEGAL_INSTRUCTION)
	{
	    have_arm_neon = FALSE;
	}
	initialized = TRUE;
    }

    return have_arm_neon;
}

#endif /* USE_ARM_NEON */

#else /* linux ELF */

#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <elf.h>

static pixman_bool_t arm_has_v7 = FALSE;
static pixman_bool_t arm_has_v6 = FALSE;
static pixman_bool_t arm_has_vfp = FALSE;
static pixman_bool_t arm_has_neon = FALSE;
static pixman_bool_t arm_has_iwmmxt = FALSE;
static pixman_bool_t arm_tests_initialized = FALSE;

static void
pixman_arm_read_auxv ()
{
    int fd;
    Elf32_auxv_t aux;

    fd = open ("/proc/self/auxv", O_RDONLY);
    if (fd >= 0)
    {
	while (read (fd, &aux, sizeof(Elf32_auxv_t)) == sizeof(Elf32_auxv_t))
	{
	    if (aux.a_type == AT_HWCAP)
	    {
		uint32_t hwcap = aux.a_un.a_val;
		/* hardcode these values to avoid depending on specific
		 * versions of the hwcap header, e.g. HWCAP_NEON
		 */
		arm_has_vfp = (hwcap & 64) != 0;
		arm_has_iwmmxt = (hwcap & 512) != 0;
		/* this flag is only present on kernel 2.6.29 */
		arm_has_neon = (hwcap & 4096) != 0;
	    }
	    else if (aux.a_type == AT_PLATFORM)
	    {
		const char *plat = (const char*) aux.a_un.a_val;
		if (strncmp (plat, "v7l", 3) == 0)
		{
		    arm_has_v7 = TRUE;
		    arm_has_v6 = TRUE;
		}
		else if (strncmp (plat, "v6l", 3) == 0)
		{
		    arm_has_v6 = TRUE;
		}
	    }
	}
	close (fd);
    }

    arm_tests_initialized = TRUE;
}

#if defined(USE_ARM_SIMD)
pixman_bool_t
pixman_have_arm_simd (void)
{
    if (!arm_tests_initialized)
	pixman_arm_read_auxv ();

    return arm_has_v6;
}

#endif /* USE_ARM_SIMD */

#if defined(USE_ARM_NEON)
pixman_bool_t
pixman_have_arm_neon (void)
{
    if (!arm_tests_initialized)
	pixman_arm_read_auxv ();

    return arm_has_neon;
}

#endif /* USE_ARM_NEON */

#endif /* linux */

#endif /* USE_ARM_SIMD || USE_ARM_NEON */

#if defined(USE_MMX) || defined(USE_SSE2)
/* The CPU detection code needs to be in a file not compiled with
 * "-mmmx -msse", as gcc would generate CMOV instructions otherwise
 * that would lead to SIGILL instructions on old CPUs that don't have
 * it.
 */
#if !defined(__amd64__) && !defined(__x86_64__) && !defined(_M_AMD64)

#ifdef HAVE_GETISAX
#include <sys/auxv.h>
#endif

typedef enum
{
    NO_FEATURES = 0,
    MMX = 0x1,
    MMX_EXTENSIONS = 0x2,
    SSE = 0x6,
    SSE2 = 0x8,
    CMOV = 0x10
} cpu_features_t;


static unsigned int
detect_cpu_features (void)
{
    unsigned int features = 0;
    unsigned int result = 0;

#ifdef HAVE_GETISAX
    if (getisax (&result, 1))
    {
	if (result & AV_386_CMOV)
	    features |= CMOV;
	if (result & AV_386_MMX)
	    features |= MMX;
	if (result & AV_386_AMD_MMX)
	    features |= MMX_EXTENSIONS;
	if (result & AV_386_SSE)
	    features |= SSE;
	if (result & AV_386_SSE2)
	    features |= SSE2;
    }
#else
    char vendor[13];
#ifdef _MSC_VER
    int vendor0 = 0, vendor1, vendor2;
#endif
    vendor[0] = 0;
    vendor[12] = 0;

#ifdef __GNUC__
    /* see p. 118 of amd64 instruction set manual Vol3 */
    /* We need to be careful about the handling of %ebx and
     * %esp here. We can't declare either one as clobbered
     * since they are special registers (%ebx is the "PIC
     * register" holding an offset to global data, %esp the
     * stack pointer), so we need to make sure they have their
     * original values when we access the output operands.
     */
    __asm__ (
        "pushf\n"
        "pop %%eax\n"
        "mov %%eax, %%ecx\n"
        "xor $0x00200000, %%eax\n"
        "push %%eax\n"
        "popf\n"
        "pushf\n"
        "pop %%eax\n"
        "mov $0x0, %%edx\n"
        "xor %%ecx, %%eax\n"
        "jz 1f\n"

        "mov $0x00000000, %%eax\n"
        "push %%ebx\n"
        "cpuid\n"
        "mov %%ebx, %%eax\n"
        "pop %%ebx\n"
        "mov %%eax, %1\n"
        "mov %%edx, %2\n"
        "mov %%ecx, %3\n"
        "mov $0x00000001, %%eax\n"
        "push %%ebx\n"
        "cpuid\n"
        "pop %%ebx\n"
        "1:\n"
        "mov %%edx, %0\n"
	: "=r" (result),
        "=m" (vendor[0]),
        "=m" (vendor[4]),
        "=m" (vendor[8])
	:
	: "%eax", "%ecx", "%edx"
        );

#elif defined (_MSC_VER)

    _asm {
	pushfd
	pop eax
	mov ecx, eax
	xor eax, 00200000h
	push eax
	popfd
	pushfd
	pop eax
	mov edx, 0
	xor eax, ecx
	jz nocpuid

	mov eax, 0
	push ebx
	cpuid
	mov eax, ebx
	pop ebx
	mov vendor0, eax
	mov vendor1, edx
	mov vendor2, ecx
	mov eax, 1
	push ebx
	cpuid
	pop ebx
    nocpuid:
	mov result, edx
    }
    memmove (vendor + 0, &vendor0, 4);
    memmove (vendor + 4, &vendor1, 4);
    memmove (vendor + 8, &vendor2, 4);

#else
#   error unsupported compiler
#endif

    features = 0;
    if (result)
    {
	/* result now contains the standard feature bits */
	if (result & (1 << 15))
	    features |= CMOV;
	if (result & (1 << 23))
	    features |= MMX;
	if (result & (1 << 25))
	    features |= SSE;
	if (result & (1 << 26))
	    features |= SSE2;
	if ((features & MMX) && !(features & SSE) &&
	    (strcmp (vendor, "AuthenticAMD") == 0 ||
	     strcmp (vendor, "Geode by NSC") == 0))
	{
	    /* check for AMD MMX extensions */
#ifdef __GNUC__
	    __asm__ (
	        "	push %%ebx\n"
	        "	mov $0x80000000, %%eax\n"
	        "	cpuid\n"
	        "	xor %%edx, %%edx\n"
	        "	cmp $0x1, %%eax\n"
	        "	jge 2f\n"
	        "	mov $0x80000001, %%eax\n"
	        "	cpuid\n"
	        "2:\n"
	        "	pop %%ebx\n"
	        "	mov %%edx, %0\n"
		: "=r" (result)
		:
		: "%eax", "%ecx", "%edx"
	        );
#elif defined _MSC_VER
	    _asm {
		push ebx
		mov eax, 80000000h
		cpuid
		xor edx, edx
		cmp eax, 1
		jge notamd
		mov eax, 80000001h
		cpuid
	    notamd:
		pop ebx
		mov result, edx
	    }
#endif
	    if (result & (1 << 22))
		features |= MMX_EXTENSIONS;
	}
    }
#endif /* HAVE_GETISAX */

    return features;
}

static pixman_bool_t
pixman_have_mmx (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t mmx_present;

    if (!initialized)
    {
	unsigned int features = detect_cpu_features ();
	mmx_present = (features & (MMX | MMX_EXTENSIONS)) == (MMX | MMX_EXTENSIONS);
	initialized = TRUE;
    }

    return mmx_present;
}

#ifdef USE_SSE2
static pixman_bool_t
pixman_have_sse2 (void)
{
    static pixman_bool_t initialized = FALSE;
    static pixman_bool_t sse2_present;

    if (!initialized)
    {
	unsigned int features = detect_cpu_features ();
	sse2_present = (features & (MMX | MMX_EXTENSIONS | SSE | SSE2)) == (MMX | MMX_EXTENSIONS | SSE | SSE2);
	initialized = TRUE;
    }

    return sse2_present;
}

#endif

#else /* __amd64__ */
#ifdef USE_MMX
#define pixman_have_mmx() TRUE
#endif
#ifdef USE_SSE2
#define pixman_have_sse2() TRUE
#endif
#endif /* __amd64__ */
#endif

pixman_implementation_t *
_pixman_choose_implementation (void)
{
#ifdef USE_SSE2
    if (pixman_have_sse2 ())
	return _pixman_implementation_create_sse2 ();
#endif
#ifdef USE_MMX
    if (pixman_have_mmx ())
	return _pixman_implementation_create_mmx ();
#endif

#ifdef USE_ARM_NEON
    if (pixman_have_arm_neon ())
	return _pixman_implementation_create_arm_neon ();
#endif
#ifdef USE_ARM_SIMD
    if (pixman_have_arm_simd ())
	return _pixman_implementation_create_arm_simd ();
#endif
#ifdef USE_VMX
    if (pixman_have_vmx ())
	return _pixman_implementation_create_vmx ();
#endif

    return _pixman_implementation_create_fast_path ();
}

