
#define SSE_FLAG  0x02000000		// SSE flag is bit 25 of Feature Flags
#define SSE2_FLAG 0x04000000		// SSE2 flag is bit 26 of Feature Flags

static bool HasCPUIDsupport()
{
	unsigned int f1, f2;
	const unsigned int flag = 0x200000;
	asm("pushfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "movl %0,%1\n\t"
	    "xorl %2,%0\n\t"
	    "pushl %0\n\t"
	    "popfl\n\t"
	    "pushfl\n\t"
	    "popl %0\n\t"
	    "popfl\n\t"
	    : "=&r" (f1), "=&r" (f2)
	    : "ir" (flag));
	return ((f1^f2) & flag) != 0;
}

static unsigned int GetFeatureFlags()
{
	int dwCPUFeatureFlags = 0x00000000;
	if (HasCPUIDsupport())
	{
		__asm__ (	"pushl %%ebx\n\t"
					"movl $1, %%eax\n\t"
					"cpuid\n\t"	
					"popl %%ebx\n\t"
				  : "=d" (dwCPUFeatureFlags) : "0" (1) : "eax", "ecx", "cc");
	}
	return dwCPUFeatureFlags;
} // GetFeatureFlags()

bool P4Available()
{
	static int checked = 0;
	if ( !checked ) {
		unsigned int dwFeatures = GetFeatureFlags();
		if (dwFeatures & SSE2_FLAG) {
			checked = 2;
			return true;
		}
		checked = 1;
	}
	if ( checked == 2 ) {
		return true;
	}
	return false;
} // IsCpuSSE2Ready()
