
// Hack to define a never-called routine from libdbm
#include "mcom_db.h"

int mkstemp(const char* /*path*/)
{
	return -1;
}
