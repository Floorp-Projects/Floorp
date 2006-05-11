%define nspr_name       nspr
%define nspr_version    4.6
%define nss_name        nss
%define nss_version     3.11
%define svrcore_name    svrcore-devel
%define svrcore_version 4.0.1
%define major           5
%define minor           17

Summary:          Mozilla LDAP C SDK
Name:             mozldap
Version:          %{major}.%{minor}
Release:          3
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/directory/csdk.html
Group:            System Environment/Libraries
Requires:         %{nspr_name} >= %{nspr_version}
Requires:         %{nss_name} >= %{nss_version}
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    %{nspr_name}-devel >= %{nspr_version}
BuildRequires:    %{nss_name}-devel >= %{nss_version}
BuildRequires:    %{svrcore_name} >= %{svrcore_version}
Provides:         mozldap

Source0:          %{name}-%{version}.tar.gz

%description
The Mozilla LDAP C SDK is a set of libraries that
allow applications to communicate with LDAP directory
servers.  These libraries are derived from the University
of Michigan and Netscape LDAP libraries.  They use Mozilla
NSPR and NSS for crypto.


%package tools
Summary:          Tools for the Mozilla LDAP C SDK
Group:            System Environment/Base
Requires:         %{name} >= %{version}-%{release}
BuildRequires:    %{nspr_name}-devel >= %{nspr_version}
BuildRequires:    %{nss_name}-devel >= %{nss_version}
BuildRequires:    %{svrcore_name} >= %{svrcore_version}
Provides:         %{name}-tools

%description tools
The mozldap-tools package provides the ldapsearch,
ldapmodify, and ldapdelete tools that use the
Mozilla LDAP C SDK libraries.


%package devel
Summary:          Development libraries and examples for Mozilla LDAP C SDK
Group:            Development/Libraries
Requires:         %{name} >= %{version}-%{release}
Requires:         %{nspr_name}-devel >= %{nspr_version}
Requires:         %{nss_name}-devel >= %{nss_version}
Provides:         %{name}-devel

%description devel
Header and Library files for doing development with the Mozilla LDAP C SDK

%prep
%setup -q
%ifarch x86_64 ppc64 ia64 s390x
arg64="--enable-64bit"
%endif
cd mozilla/directory/c-sdk
./configure $arg64 --with-system-svrcore --enable-optimize --disable-debug

%build

# Enable compiler optimizations and disable debugging code
BUILD_OPT=1
export BUILD_OPT

# Generate symbolic info for debuggers
XCFLAGS=$RPM_OPT_FLAGS
export XCFLAGS

PKG_CONFIG_ALLOW_SYSTEM_LIBS=1
PKG_CONFIG_ALLOW_SYSTEM_CFLAGS=1

export PKG_CONFIG_ALLOW_SYSTEM_LIBS
export PKG_CONFIG_ALLOW_SYSTEM_CFLAGS

%ifarch x86_64 ppc64 ia64 s390x
USE_64=1
export USE_64
%endif

cd mozilla/directory/c-sdk
make BUILDCLU=1 HAVE_SVRCORE=1 BUILD_OPT=1

# Set up our package file
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}/pkgconfig
%{__cat} %{name}.pc.in | sed -e "s,%%libdir%%,%{_libdir},g" \
                          -e "s,%%prefix%%,%{_prefix},g" \
                          -e "s,%%exec_prefix%%,%{_prefix},g" \
                          -e "s,%%includedir%%,%{_includedir}/%{name},g" \
                          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
                          -e "s,%%NSS_VERSION%%,%{nss_version},g" \
                          -e "s,%%SVRCORE_VERSION%%,%{svrcore_version},g" \
                          -e "s,%%MOZLDAP_VERSION%%,%{version},g" > \
                          $RPM_BUILD_ROOT/%{_libdir}/pkgconfig/%{name}.pc

%install

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT/%{_includedir}/%{name}
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}/%{name}

# Copy the binary libraries we want
for file in libssldap50.so libprldap50.so libldap50.so
do
  %{__install} -m 755 mozilla/dist/lib/$file $RPM_BUILD_ROOT/%{_libdir}
done

# Copy the binaries we want
for file in ldapsearch ldapmodify ldapdelete ldapcmp ldapcompare
do
  %{__install} -m 755 mozilla/dist/bin/$file $RPM_BUILD_ROOT/%{_libdir}/%{name}
done

# Copy the include files
for file in mozilla/dist/public/ldap/*.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT/%{_includedir}/%{name}
done

# Copy the developer files
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/%{name}
cp -r mozilla/directory/c-sdk/ldap/examples $RPM_BUILD_ROOT%{_datadir}/%{name}
%{__mkdir_p} $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/examples/xmplflt.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldaptemplates.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapfilter.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapsearchprefs.conf $RPM_BUILD_ROOT%{_datadir}/%{name}/etc

# Rename the libraries and create the symlinks
cd $RPM_BUILD_ROOT/%{_libdir}
for file in libssldap50.so libprldap50.so libldap50.so
do
  mv $file $file.%{major}.%{minor}
  ln -s $file.%{major}.%{minor} $file.%{major}
  ln -s $file.%{major} $file
done

%clean
%{__rm} -rf $RPM_BUILD_ROOT


%post
/sbin/ldconfig >/dev/null 2>/dev/null


%postun
/sbin/ldconfig >/dev/null 2>/dev/null


%files
%defattr(0755,root,root)
%{_libdir}/libssldap50.so
%{_libdir}/libprldap50.so
%{_libdir}/libldap50.so
%{_libdir}/libssldap50.so.%{major}
%{_libdir}/libprldap50.so.%{major}
%{_libdir}/libldap50.so.%{major}
%{_libdir}/libssldap50.so.%{major}.%{minor}
%{_libdir}/libprldap50.so.%{major}.%{minor}
%{_libdir}/libldap50.so.%{major}.%{minor}

%files tools
%defattr(0755,root,root)
%attr(0755,root,root) %dir %{_libdir}/%{name}
%{_libdir}/%{name}/ldapsearch
%{_libdir}/%{name}/ldapmodify
%{_libdir}/%{name}/ldapdelete
%{_libdir}/%{name}/ldapcmp
%{_libdir}/%{name}/ldapcompare

%files devel
%defattr(0644,root,root)
%{_libdir}/pkgconfig/%{name}.pc
%attr(0755,root,root) %dir %{_includedir}/%{name}
%{_includedir}/%{name}
%attr(0755,root,root) %dir %{_datadir}/%{name}
%attr(0755,root,root) %dir %{_datadir}/%{name}/etc
%attr(0755,root,root) %dir %{_datadir}/%{name}/examples
%{_datadir}/%{name}

%changelog
* Tue Apr 18 2006 Richard Megginson <richm@stanforalumni.org> - 5.17-3
- make more Fedora Core friendly - move each requires and buildrequires to a separate line
- remove --with-nss since svrcore implies it; fix some macro errors; macro-ize nspr and nss names
- fix directory attrs in devel package

* Tue Jan 31 2006 Rich Megginson <rmeggins@redhat.com> - 5.17-2
- use --with-system-svrcore to configure

* Mon Dec 19 2005 Rich Megginson <rmeggins@redhat.com> - 5.17-1
- Initial revision

