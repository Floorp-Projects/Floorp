// expected-no-diagnostics

#define MOZ_NEEDS_MEMMOVABLE_TYPE __attribute__((annotate("moz_needs_memmovable_type")))

template<class T>
class MOZ_NEEDS_MEMMOVABLE_TYPE Mover { T mForceInst; };

#include <atomic>
#include <cstdint>
struct CustomType{};
static struct {
  Mover<std::atomic<CustomType>>         m1;
  Mover<std::atomic<bool>>               m2;
  Mover<std::atomic<char>>               m3;
  Mover<std::atomic<signed char>>        m4;
  Mover<std::atomic<unsigned char>>      m5;
  Mover<std::atomic<char16_t>>           m6;
  Mover<std::atomic<char32_t>>           m7;
  Mover<std::atomic<wchar_t>>            m8;
  Mover<std::atomic<short>>              m9;
  Mover<std::atomic<unsigned short>>     m10;
  Mover<std::atomic<int>>                m11;
  Mover<std::atomic<unsigned int>>       m12;
  Mover<std::atomic<long>>               m13;
  Mover<std::atomic<unsigned long>>      m14;
  Mover<std::atomic<long long>>          m15;
  Mover<std::atomic<unsigned long long>> m16;
  Mover<std::atomic<void*>>              m17;
  Mover<std::atomic<CustomType*>>        m18;
} good;
