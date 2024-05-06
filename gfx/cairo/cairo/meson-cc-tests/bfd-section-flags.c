#include <bfd.h>

int
f (asection *s)
{
  return bfd_section_flags(s) == 0;
}

int main (void) { return 0; }
