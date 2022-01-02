#include "gdb-tests.h"

#include <stdint.h>

enum unscoped_no_storage { EnumValue1 };

enum unscoped_with_storage : uint8_t { EnumValue2 };

enum class scoped_no_storage { EnumValue3 };

enum class scoped_with_storage : uint8_t { EnumValue4 };

FRAGMENT(enum_printers, one) {
  unscoped_no_storage i1 = EnumValue1;
  unscoped_with_storage i2 = EnumValue2;
  scoped_no_storage i3 = scoped_no_storage::EnumValue3;
  scoped_with_storage i4 = scoped_with_storage::EnumValue4;

  breakpoint();

  use(i1);
  use(i2);
  use(i3);
  use(i4);
}
