#include <stdint.h>
#include "gtest/gtest.h"

extern "C" uint8_t* test_rust();

TEST(rust, CallFromCpp) {
  auto greeting = test_rust();
  EXPECT_STREQ(reinterpret_cast<char*>(greeting), "hello from rust.");
}
