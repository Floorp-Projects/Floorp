/* Define to `int' if <sys/types.h> doesn't define.  */
#undef ssize_t

/* Define if you want a debugging version. */
#undef DEBUG

/* Define if you want a version with run-time diagnostic checking. */
#undef DIAGNOSTIC

/* Define if you have sigfillset (and sigprocmask). */
#undef HAVE_SIGFILLSET

/* Define if building on AIX, HP, Solaris to get big-file environment. */
#undef	HAVE_FILE_OFFSET_BITS
#ifdef	HAVE_FILE_OFFSET_BITS
#define	_FILE_OFFSET_BITS	64
#endif

/* Define if you have spinlocks. */
#undef HAVE_SPINLOCKS

/* Define if you want to use mc68020/gcc assembly spinlocks. */
#undef HAVE_ASSEM_MC68020_GCC

/* Define if you want to use parisc/gcc assembly spinlocks. */
#undef HAVE_ASSEM_PARISC_GCC

/* Define if you want to use sco/cc assembly spinlocks. */
#undef HAVE_ASSEM_SCO_CC

/* Define if you want to use sparc/gcc assembly spinlocks. */
#undef HAVE_ASSEM_SPARC_GCC

/* Define if you want to use uts4/cc assembly spinlocks. */
#undef HAVE_ASSEM_UTS4_CC

/* Define if you want to use x86/gcc assembly spinlocks. */
#undef HAVE_ASSEM_X86_GCC

/* Define if you have the AIX _check_lock spinlocks. */
#undef HAVE_FUNC_AIX

/* Define if you have the OSF1 or HPPA msemaphore spinlocks. */
#undef HAVE_FUNC_MSEM

/* Define if you have the SGI abilock_t spinlocks. */
#undef HAVE_FUNC_SGI

/* Define if you have the ReliantUNIX spinlock_t spinlocks. */
#undef HAVE_FUNC_RELIANT

/* Define if you have the Solaris mutex_t spinlocks. */
#undef HAVE_FUNC_SOLARIS

/* Define if you have to initialize the entire shared region. */
#undef REGION_INIT_NEEDED

/* Define if your sprintf returns a pointer, not a length. */
#undef SPRINTF_RET_CHARPNT
