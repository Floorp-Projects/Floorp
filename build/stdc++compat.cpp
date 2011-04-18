/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is a hack to avoid dependencies on recent libstdc++.
 *
 * The Initial Developer of the Original Code is
 * Mozilla Foundation.
 * Portions created by the Initial Developer are Copyright (C) 2011
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Mike Hommey <mh@glandium.org>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include <ostream>
#include <istream>
#ifdef DEBUG
#include <string>
#endif

namespace std {
#if (__GNUC__ == 4) && (__GNUC_MINOR__ >= 2)
    /* Instantiate these templates to avoid GLIBCXX_3.4.9 symbol versions */
    template ostream& ostream::_M_insert(double);
    template ostream& ostream::_M_insert(long);
    template ostream& ostream::_M_insert(unsigned long);
    template ostream& __ostream_insert(ostream&, const char*, streamsize);
    template istream& istream::_M_extract(double&);
#endif
#ifdef DEBUG
#if (__GNUC__ == 4) && (__GNUC_MINOR__ >= 5)
    /* Instantiate these templates to avoid GLIBCXX_3.4.14 symbol versions
     * in debug builds */
    template char *basic_string<char, char_traits<char>, allocator<char> >::_S_construct_aux_2(size_type, char, allocator<char> const&);
    template wchar_t *basic_string<wchar_t, char_traits<wchar_t>, allocator<wchar_t> >::_S_construct_aux_2(size_type, wchar_t, allocator<wchar_t> const&);
#endif
#endif
}

namespace std __attribute__((visibility("default"))) {

#if (__GNUC__ == 4) && (__GNUC_MINOR__ >= 5)
    /* Hack to avoid GLIBCXX_3.4.14 symbol versions */
    struct _List_node_base
    {
        void hook(_List_node_base * const __position) throw ();

        void unhook() throw ();

        void transfer(_List_node_base * const __first,
                      _List_node_base * const __last) throw();

        void _M_hook(_List_node_base * const __position) throw ();

        void _M_unhook() throw ();

        void _M_transfer(_List_node_base * const __first,
                         _List_node_base * const __last) throw();
    };

    /* The functions actually have the same implementation */
    void
    _List_node_base::_M_hook(_List_node_base * const __position) throw ()
    {
        hook(__position);
    }

    void
    _List_node_base::_M_unhook() throw ()
    {
        unhook();
    }

    void
    _List_node_base::_M_transfer(_List_node_base * const __first,
                                 _List_node_base * const __last) throw ()
    {
        transfer(__first, __last);
    }
#endif

#if (__GNUC__ == 4) && (__GNUC_MINOR__ >= 4)
    /* Hack to avoid GLIBCXX_3.4.11 symbol versions
       An inline definition of ctype<char>::_M_widen_init() used to be in
       locale_facets.h before GCC 4.4, but moved out of headers in more
       recent versions.
       It is actually safe to make it do nothing. */
    void ctype<char>::_M_widen_init() const {}
#endif

}
