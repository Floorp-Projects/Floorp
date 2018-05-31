hidden {
  local:
    # std::func()
    _ZSt*;
    # std::namespace::func()
    _ZNSt*;
    # std::namespace::{lambda}
    _ZZNSt*;
    # std::namespace::func() const
    _ZNKSt*;
    # std::namespace::func() const::global
    _ZZNKSt*;
    # guard variable for std::namespace::func() const::global
    _ZGVZNKSt*;
    # std::allocator
    _ZNSa*;
    # std::basic_string
    _ZNSb*;
    # std::basic_string<...>
    _ZNSs*;
    # std::basic_istream<...>
    _ZNSi*;
    # std::basic_ostream<...>
    _ZNSo*;
    # std::basic_iostream<...>
    _ZNSd*;
    # vtable for std::...
    _ZTVNSt*;
};
