/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <ostream>
#include <istream>
#include <string>

/* GLIBCXX_3.4.8  is from gcc 4.1.1 (111691)
   GLIBCXX_3.4.9  is from gcc 4.2.0 (111690)
   GLIBCXX_3.4.10 is from gcc 4.3.0 (126287)
   GLIBCXX_3.4.11 is from gcc 4.4.0 (133006)
   GLIBCXX_3.4.12 is from gcc 4.4.1 (147138)
   GLIBCXX_3.4.13 is from gcc 4.4.2 (151127)
   GLIBCXX_3.4.14 is from gcc 4.5.0 (151126)
   GLIBCXX_3.4.15 is from gcc 4.6.0 (160071)
   GLIBCXX_3.4.16 is form gcc 4.6.1 (172240) */

#define GLIBCXX_VERSION(a, b, c) (((a) << 16) | ((b) << 8) | (c))

namespace std {
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 9)
    /* Instantiate these templates to avoid GLIBCXX_3.4.9 symbol versions */
    template ostream& ostream::_M_insert(double);
    template ostream& ostream::_M_insert(long);
    template ostream& ostream::_M_insert(unsigned long);
    template ostream& ostream::_M_insert(long long);
    template ostream& ostream::_M_insert(unsigned long long);
    template ostream& ostream::_M_insert(bool);
    template ostream& ostream::_M_insert(const void*);
    template ostream& __ostream_insert(ostream&, const char*, streamsize);
    template istream& istream::_M_extract(double&);
    template istream& istream::_M_extract(float&);
    template istream& istream::_M_extract(unsigned int&);
    template istream& istream::_M_extract(unsigned long&);
    template istream& istream::_M_extract(unsigned short&);
    template istream& istream::_M_extract(unsigned long long&);
#endif
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)
    /* Instantiate these templates to avoid GLIBCXX_3.4.14 symbol versions
     * depending on optimization level */
    template char *string::_S_construct_aux_2(size_type, char, allocator<char> const&);
#ifdef _GLIBCXX_USE_WCHAR_T
    template wchar_t *wstring::_S_construct_aux_2(size_type, wchar_t, allocator<wchar_t> const&);
#endif /* _GLIBCXX_USE_WCHAR_T */
#ifdef __GXX_EXPERIMENTAL_CXX0X__
    template string::basic_string(string&&);
    template string& string::operator=(string&&);
    template wstring::basic_string(wstring&&);
    template wstring& wstring::operator=(wstring&&);
    template string& string::assign(string&&);
    template wstring& wstring::assign(wstring&&);
#endif /* __GXX_EXPERIMENTAL_CXX0X__ */
#endif /* (__GNUC__ == 4) && (__GNUC_MINOR__ >= 5) */
}

namespace std __attribute__((visibility("default"))) {
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)
    /* Hack to avoid GLIBCXX_3.4.14 symbol versions */
    struct _List_node_base
    {
        void hook(_List_node_base * const __position) throw ();

        void unhook() throw ();

        void transfer(_List_node_base * const __first,
                      _List_node_base * const __last) throw();

        void reverse() throw();

/* Hack to avoid GLIBCXX_3.4.15 symbol versions */
#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
        static void swap(_List_node_base& __x, _List_node_base& __y) throw ();
    };

    namespace __detail {

    struct _List_node_base
    {
#endif
        void _M_hook(_List_node_base * const __position) throw ();

        void _M_unhook() throw ();

        void _M_transfer(_List_node_base * const __first,
                         _List_node_base * const __last) throw();

        void _M_reverse() throw();

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
        static void swap(_List_node_base& __x, _List_node_base& __y) throw ();
#endif
    };

    /* The functions actually have the same implementation */
    void
    _List_node_base::_M_hook(_List_node_base * const __position) throw ()
    {
        ((std::_List_node_base *)this)->hook((std::_List_node_base * const) __position);
    }

    void
    _List_node_base::_M_unhook() throw ()
    {
        ((std::_List_node_base *)this)->unhook();
    }

    void
    _List_node_base::_M_transfer(_List_node_base * const __first,
                                 _List_node_base * const __last) throw ()
    {
        ((std::_List_node_base *)this)->transfer((std::_List_node_base * const)__first,
                                                 (std::_List_node_base * const)__last);
    }

    void
    _List_node_base::_M_reverse() throw ()
    {
        ((std::_List_node_base *)this)->reverse();
    }

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 15)
    void
    _List_node_base::swap(_List_node_base& __x, _List_node_base& __y) throw ()
    {
        std::_List_node_base::swap(*((std::_List_node_base *) &__x),
                                   *((std::_List_node_base *) &__y));
    }
}
#endif

#endif /*MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 14)*/

#if MOZ_LIBSTDCXX_VERSION >= GLIBCXX_VERSION(3, 4, 11)
    /* Hack to avoid GLIBCXX_3.4.11 symbol versions
       An inline definition of ctype<char>::_M_widen_init() used to be in
       locale_facets.h before GCC 4.4, but moved out of headers in more
       recent versions.
       It is actually safe to make it do nothing. */
    void ctype<char>::_M_widen_init() const {}
#endif

}
