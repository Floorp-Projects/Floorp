# ***** BEGIN LICENSE BLOCK *****
# Version: MPL 1.1/GPL 2.0/LGPL 2.1
# 
# The contents of this file are subject to the Mozilla Public License Version 
# 1.1 (the "License"); you may not use this file except in compliance with 
# the License. You may obtain a copy of the License at 
# http://www.mozilla.org/MPL/
# 
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
# 
# The Original Code is Mozilla Communicator client code.
# 
# The Initial Developer of the Original Code is
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1996-1999
# the Initial Developer. All Rights Reserved.
# 
# Contributor(s):
# 
# Alternatively, the contents of this file may be used under the terms of
# either of the GNU General Public License Version 2 or later (the "GPL"),
# or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the MPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the MPL, the GPL or the LGPL.
# 
# ***** END LICENSE BLOCK ***** 

10	ldap_abandon
11	ldap_add
13	ldap_unbind

#14	ldap_enable_cache
#15	ldap_disable_cache
#16	ldap_destroy_cache
#17	ldap_flush_cache
#18	ldap_uncache_entry

19	ldap_compare
20	ldap_delete
21	ldap_result2error
22	ldap_err2string
23	ldap_modify
24	ldap_modrdn
25	ldap_open
26	ldap_first_entry
27	ldap_next_entry
30	ldap_get_dn
31	ldap_dn2ufn
32	ldap_first_attribute
33	ldap_next_attribute
34	ldap_get_values
35	ldap_get_values_len
36	ldap_count_entries
37	ldap_count_values
38	ldap_value_free
39	ldap_explode_dn
40	ldap_result
41	ldap_msgfree
43	ldap_search
44	ldap_add_s
45	ldap_bind_s
46	ldap_unbind_s
47	ldap_delete_s
48	ldap_modify_s
49	ldap_modrdn_s
50	ldap_search_s
51	ldap_search_st
52	ldap_compare_s
53	ldap_ufn_search_c
54	ldap_ufn_search_s
55	ldap_init_getfilter
56	ldap_getfilter_free
57	ldap_getfirstfilter
58	ldap_getnextfilter
59	ldap_simple_bind
60	ldap_simple_bind_s
61	ldap_bind
62	ldap_friendly_name
63	ldap_free_friendlymap
64	ldap_ufn_search_ct

#65	ldap_set_cache_options
#66	ldap_uncache_request

67	ldap_modrdn2
68	ldap_modrdn2_s
69	ldap_ufn_setfilter
70	ldap_ufn_setprefix
71	ldap_ufn_timeout	C
72	ldap_init_getfilter_buf
73	ldap_setfilteraffixes
74	ldap_sort_entries
75	ldap_sort_values
76	ldap_sort_strcasecmp	C
77	ldap_count_values_len
78	ldap_name2template
79	ldap_value_free_len

# manually comment and uncomment these five as necessary
#80	ldap_kerberos_bind1
#81	ldap_kerberos_bind2
#82	ldap_kerberos_bind_s
#83	ldap_kerberos_bind1_s
#84	ldap_kerberos_bind2_s

85	ldap_init
86	ldap_is_dns_dn
87	ldap_explode_dns
88	ldap_mods_free

89	ldap_is_ldap_url
90	ldap_free_urldesc
91	ldap_url_parse
92	ldap_url_search
93	ldap_url_search_s
94	ldap_url_search_st
95	ldap_set_rebind_proc
100	ber_skip_tag
101	ber_peek_tag
102	ber_get_int
103	ber_get_stringb
104	ber_get_stringa
105	ber_get_stringal
106	ber_get_bitstringa
107	ber_get_null
108	ber_get_boolean
109	ber_first_element
110	ber_next_element
111	ber_scanf	C
112	ber_bvfree
113	ber_bvecfree
114	ber_put_int
115	ber_put_ostring
116	ber_put_string
117	ber_put_bitstring
118	ber_put_null
119	ber_put_boolean
120	ber_start_seq
121	ber_start_set
122	ber_put_seq
123	ber_put_set
124	ber_printf	C
125	ber_read
126	ber_write
127	ber_free
128	ber_flush
129	ber_alloc
130	ber_dup
131	ber_get_next
132	ber_get_tag
133	ber_put_enum
134	der_alloc
135	ber_alloc_t
136	ber_bvdup
137	ber_init_w_nullchar
138	ber_reset
139	ber_get_option
140	ber_set_option
141	ber_sockbuf_alloc
142	ber_sockbuf_get_option
143	ber_sockbuf_set_option
144	ber_init
145	ber_flatten
146	ber_special_alloc
147	ber_special_free
148	ber_get_next_buffer
149	ber_err_print		C
150	ber_sockbuf_free
151	ber_get_next_buffer_ext
152	ber_svecfree
153	ber_get_buf_datalen
154	ber_get_buf_databegin
155	ber_stack_init
156	ber_sockbuf_free_data

200	ldap_memfree
201	ldap_ber_free

300	ldap_init_searchprefs
301	ldap_init_searchprefs_buf
302	ldap_free_searchprefs
303	ldap_first_searchobj
304	ldap_next_searchobj
305	ldap_build_filter

400	ldap_init_templates
401	ldap_init_templates_buf
402	ldap_free_templates
403	ldap_first_disptmpl
404	ldap_next_disptmpl
405	ldap_oc2template
406	ldap_tmplattrs
407	ldap_first_tmplrow
408	ldap_next_tmplrow
409	ldap_first_tmplcol
410	ldap_next_tmplcol
411	ldap_entry2text_search
412	ldap_entry2text
413	ldap_vals2text
414	ldap_entry2html
415	ldap_entry2html_search
416	ldap_vals2html
417	ldap_tmplerr2string
418	ldap_set_option
419	ldap_get_option
420	ldap_charray_merge
430	ldap_get_lderrno
431	ldap_set_lderrno
432	ldap_perror
433	ldap_set_filter_additions
434	ldap_create_filter
440	ldap_version
441	ldap_multisort_entries
442	ldap_msgid
443	ldap_explode_rdn
444	ldap_msgtype
445	ldap_cache_flush
446	ldap_str2charray
447	ldap_charray_add
448	ldap_charray_dup
449	ldap_charray_free

# Windows ordinals 450-469 are reserved for SSL routines

470	ldap_charray_inlist
471	ldap_charray_position
472	ldap_rename
473	ldap_rename_s
474	ldap_utf8len
475	ldap_utf8next
476	ldap_utf8prev
477	ldap_utf8copy
478	ldap_utf8characters
479	ldap_utf8strtok_r
480	ldap_utf8isalnum
481	ldap_utf8isalpha
482	ldap_utf8isdigit
483	ldap_utf8isxdigit
484	ldap_utf8isspace
485	ldap_control_free
486	ldap_controls_free
487	ldap_sasl_bind
488	ldap_sasl_bind_s
489	ldap_parse_sasl_bind_result
490	ldap_sasl_interactive_bind_s
491	ldap_sasl_interactive_bind_ext_s
# LDAPv3 simple paging controls are not supported by Netscape at this time.
# 490	ldap_create_page_control
# 491	ldap_parse_page_control
492	ldap_create_sort_control
493	ldap_parse_sort_control
# an LDAPv3 language control was proposed but then retracted.
# 494	ldap_create_language_control
495	ldap_get_lang_values
496	ldap_get_lang_values_len
497	ldap_free_sort_keylist
498	ldap_create_sort_keylist
499	ldap_utf8getcc
500	ldap_get_entry_controls
501	ldap_create_persistentsearch_control
502	ldap_parse_entrychange_control
503	ldap_parse_result
504	ldap_parse_extended_result
505	ldap_parse_reference
506	ldap_abandon_ext
507	ldap_add_ext
508	ldap_add_ext_s
509	ldap_modify_ext
510	ldap_modify_ext_s
511	ldap_first_message
512	ldap_next_message
513	ldap_compare_ext
514	ldap_compare_ext_s
515	ldap_delete_ext
516	ldap_delete_ext_s
517	ldap_search_ext
518	ldap_search_ext_s
519	ldap_extended_operation
520	ldap_extended_operation_s
521	ldap_first_reference
522	ldap_next_reference
523	ldap_count_references
524	ldap_count_messages
525	ldap_create_virtuallist_control
526	ldap_parse_virtuallist_control
527	ldap_create_proxyauth_control
528	ldap_unbind_ext
529	ldap_x_hostlist_first
530	ldap_x_hostlist_next
531	ldap_x_hostlist_statusfree
532	ldap_x_malloc
533	ldap_x_calloc
534	ldap_x_realloc
535	ldap_x_free
#
536 ldap_create_proxiedauth_control
#
537 ldap_create_geteffectiveRights_control 
#
538 ldap_find_control
#
539 ldap_url_parse_no_defaults
#
550 ldap_create_userstatus_control
551 ldap_parse_userstatus_control
#
560 ldap_create_passwordpolicy_control
561 ldap_create_passwordpolicy_control_ext
562 ldap_parse_passwordpolicy_control
563 ldap_parse_passwordpolicy_control_ext
564 ldap_passwordpolicy_err2txt
#
570 ldap_passwd
571 ldap_parse_passwd
573 ldap_passwd_s
#
580 ldap_delete_result_entry
581 ldap_add_result_entry
#
590 ldap_whoami
591 ldap_parse_whoami
592 ldap_whoami_s
#
600 ldap_create_authzid_control
601 ldap_parse_authzid_control
#
1000	ldap_memcache_init
1001	ldap_memcache_set
1002	ldap_memcache_get
1003	ldap_memcache_flush
1004	ldap_memcache_destroy
1005	ldap_memcache_update
1006	ldap_keysort_entries
0	ldap_debug	G	full
0	lber_debug	G	full
