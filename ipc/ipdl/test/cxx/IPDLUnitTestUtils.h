
#ifndef mozilla__ipdltest_IPDLUnitTestUtils
#define mozilla__ipdltest_IPDLUnitTestUtils 1

#include "ipc/IPCMessageUtils.h"

namespace mozilla {
namespace _ipdltest {

struct Bad {};

}  // namespace _ipdltest
}  // namespace mozilla

namespace IPC {

template <>
struct ParamTraits<mozilla::_ipdltest::Bad> {
  typedef mozilla::_ipdltest::Bad paramType;

  // Defined in TestActorPunning.cpp.
  static void Write(MessageWriter* aWriter, const paramType& aParam);
  static bool Read(MessageReader* aReader, paramType* aResult);
};

}  // namespace IPC

#endif  // mozilla__ipdltest_IPDLUnitTestUtils
