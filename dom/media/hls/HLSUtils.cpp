#include "HLSUtils.h"

mozilla::LogModule* GetHLSLog()
{
  static mozilla::LazyLogModule sLogModule("HLS");
  return sLogModule;
}