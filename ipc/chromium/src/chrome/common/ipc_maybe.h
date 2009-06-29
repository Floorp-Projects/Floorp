#ifndef CHROME_COMMON_MAYBE_H_
#define CHROME_COMMON_MAYBE_H_

namespace IPC {

// The Maybe type can be used to avoid serialising a type when it's invalid.
// This is most useful in conjunction with FileDescriptor, as there's no
// possible invalid value which can be serialised (one can type to use -1, but
// the IPC channel will fail). It may also be useful if the invalid value is
// otherwise expensive to serialise.
template<typename A>
struct Maybe {
  bool valid;
  A value;
};

}  // namespace IPC

#endif  // CHROME_COMMON_MAYBE_H_
