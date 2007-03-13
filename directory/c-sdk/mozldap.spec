%define nspr_name       nspr
%define nspr_version    4.6
%define nss_name        nss
%define nss_version     3.11
%define svrcore_name    svrcore
%define svrcore_version 4.0.3

%define major           6
%define minor           0
%define submin          3
%define libsuffix       %{major}0

Summary:          Mozilla LDAP C SDK
Name:             mozldap
Version:          %{major}.%{minor}.%{submin}
Release:          1%{?dist}
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/directory/csdk.html
Group:            System Environment/Libraries
Requires:         %{nspr_name} >= %{nspr_version}
Requires:         %{nss_name} >= %{nss_version}
Requires:         %{svrcore_name} >= %{svrcore_version}
Requires:         cyrus-sasl-lib
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    %{nspr_name}-devel >= %{nspr_version}
BuildRequires:    %{nss_name}-devel >= %{nss_version}
BuildRequires:    %{svrcore_name}-devel >= %{svrcore_version}
BuildRequires:    gcc-c++
BuildRequires:    cyrus-sasl-devel

Source0:          ftp://ftp.mozilla.org/pub/mozilla.org/directory/c-sdk/releases/v%{version}/src/%{name}-%{version}.tar.gz

%description
The Mozilla LDAP C SDK is a set of libraries that
allow applications to communicate with LDAP directory
servers.  These libraries are derived from the University
of Michigan and Netscape LDAP libraries.  They use Mozilla
NSPR and NSS for crypto.


%package tools
Summary:          Tools for the Mozilla LDAP C SDK
Group:            System Environment/Base
Requires:         %{name} = %{version}-%{release}
BuildRequires:    %{nspr_name}-devel >= %{nspr_version}
BuildRequires:    %{nss_name}-devel >= %{nss_version}
BuildRequires:    %{svrcore_name}-devel >= %{svrcore_version}

%description tools
The mozldap-tools package provides the ldapsearch,
ldapmodify, and ldapdelete tools that use the
Mozilla LDAP C SDK libraries.


%package devel
Summary:          Development libraries and examples for Mozilla LDAP C SDK
Group:            Development/Libraries
Requires:         %{name} = %{version}-%{release}
Requires:         %{nspr_name}-devel >= %{nspr_version}
Requires:         %{nss_name}-devel >= %{nss_version}
Requires:         %{svrcore_name}-devel >= %{svrcore_version}

%description devel
Header and Library files for doing development with the Mozilla LDAP C SDK

%prep
%setup -q

%build
cd mozilla/directory/c-sdk

%configure \
%ifarch x86_64 ppc64 ia64 s390x
    --enable-64bit \
%endif
    --with-sasl \
    --enable-clu \
    --with-system-svrcore \
    --enable-optimize \
    --disable-debug

# Enable compiler optimizations and disable debugging code
BUILD_OPT=1
export BUILD_OPT

# Generate symbolic info for debuggers
XCFLAGS="$RPM_OPT_FLAGS"
export XCFLAGS

PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1

export PKG_CONFIG_ALLOW_SYSTEM_LIBS
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS

make \
%ifarch x86_64 ppc64 ia64 s390x
    USE_64=1
%endif

%install
%{__rm} -rf $RPM_BUILD_ROOT

# Set up our package file
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/pkgconfig
%{__cat} mozilla/directory/c-sdk/mozldap.pc.in \
    | sed -e "s,%%libdir%%,%{_libdir},g" \
          -e "s,%%prefix%%,%{_prefix},g" \
          -e "s,%%major%%,%{major},g" \
          -e "s,%%minor%%,%{minor},g" \
          -e "s,%%submin%%,%{submin},g" \
          -e "s,%%libsuffix%%,%{libsuffix},g" \
          -e "s,%%bindir%%,%{_libdir}/%{name},g" \
          -e "s,%%exec_prefix%%,%{_prefix},g" \
          -e "s,%%includedir%%,%{_includedir}/%{name},g" \
          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
          -e "s,%%NSS_VERSION%%,%{nss_version},g" \
          -e "s,%%SVRCORE_VERSION%%,%{svrcore_version},g" \
          -e "s,%%MOZLDAP_VERSION%%,%{version},g" \
    > $RPM_BUILD_ROOT%{_libdir}/pkgconfig/%{name}.pc

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT%{_includedir}/%{name}
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/%{name}

# Copy the binary libraries we want
for file in libssldap%{libsuffix}.so libprldap%{libsuffix}.so libldap%{libsuffix}.so libldif%{libsuffix}.so
do
  %{__install} -m 755 mozilla/dist/lib/$file $RPM_BUILD_ROOT%{_libdir}
done

# Copy the binaries we want
for file in ldapsearch ldapmodify ldapdelete ldapcmp ldapcompare ldappasswd
do
  %{__install} -m 755 mozilla/dist/bin/$file $RPM_BUILD_ROOT%{_libdir}/%{name}
done

# Copy the include files
for file in mozilla/dist/public/ldap/*.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT%{_includedir}/%{name}
done

# Copy the developer files
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/%{name}
cp -r mozilla/directory/c-sdk/ldap/examples $RPM_BUILD_ROOT%{_datadir}/%{name}
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/examples/xmplflt.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldaptemplates.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapfilter.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapsearchprefs.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc

%clean
%{__rm} -rf $RPM_BUILD_ROOT


%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc mozilla/directory/c-sdk/README.rpm
%{_libdir}/libssldap*.so
%{_libdir}/libprldap*.so
%{_libdir}/libldap*.so
%{_libdir}/libldif*.so

%files tools
%defattr(-,root,root,-)
%dir %{_libdir}/%{name}
%{_libdir}/%{name}/ldapsearch
%{_libdir}/%{name}/ldapmodify
%{_libdir}/%{name}/ldapdelete
%{_libdir}/%{name}/ldapcmp
%{_libdir}/%{name}/ldapcompare
%{_libdir}/%{name}/ldappasswd

%files devel
%defattr(-,root,root,-)
%{_libdir}/pkgconfig/%{name}.pc
%{_includedir}/%{name}
%{_datadir}/%{name}

%changelog
* Tue Mar 13 2007 Rich Megginson <richm@stanfordalumni.org> - 6.0.3-1
- bumped version to 6.0.3
- minor build fixes for some platforms

* Mon Jan 15 2007 Rich Megginson <richm@stanfordalumni.org> - 6.0.2-1
- Fixed exports file generation for Solaris and Windows - no effect on linux
- bumped version to 6.0.2

* Mon Jan  9 2007 Rich Megginson <richm@stanfordalumni.org> - 6.0.1-2
- Remove buildroot = "/" checking
- Remove buildroot removal from %%build section

* Mon Jan  8 2007 Rich Megginson <richm@stanfordalumni.org> - 6.0.1-1
- bump version to 6.0.1
- added libldif and ldif.h

* Fri Dec  8 2006 Axel Thimm <Axel.Thimm@ATrpms.net> - 6.0.0-2
- Rename to mozldap.
- move configure step to %%build section.
- clean up excessive use of %%defines, make more Fedora like.
- fix mismatching soname issue.
- generic specfile cosmetics.

* Thu Oct  5 2006 Rich Megginson <richm@stanforalumni.org> - 6.0.0-1
- Bump version to 6.0.0 - add support for submit/patch level (3rd level) in version numbering

* Tue Apr 18 2006 Richard Megginson <richm@stanforalumni.org> - 5.17-3
- make more Fedora Core friendly - move each requires and buildrequires to a separate line
- remove --with-nss since svrcore implies it; fix some macro errors; macro-ize nspr and nss names
- fix directory attrs in devel package

* Tue Jan 31 2006 Rich Megginson <rmeggins@redhat.com> - 5.17-2
- use --with-system-svrcore to configure

* Mon Dec 19 2005 Rich Megginson <rmeggins@redhat.com> - 5.17-1
- Initial revision

