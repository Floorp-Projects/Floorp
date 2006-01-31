%define nspr_version 4.6
%define nss_version 3.11
%define svrcore_version 4.0.1
%define major 5
%define minor 17

Summary:          Mozilla LDAP C SDK
Name:             mozldap
Version:          %{major}.%{minor}
Release:          2
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/directory/csdk.html
Group:            System Environment/Libraries
Requires:         nspr >= %{nspr_version}, nss >= %{nss_version}
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    nspr-devel >= %{nspr_version}, nss-devel >= %{nss_version}, svrcore-devel >= %{svrcore_version}
BuildRequires:    pkgconfig
BuildRequires:    gawk
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
Requires:         mozldap = %{version}-%{release}
BuildRequires:    nspr-devel >= %{nspr_version}, nss-devel >= %{nss_version}, svrcore-devel >= %{svrcore_version}
Provides:         mozldap-tools

%description tools
The mozldap-tools package provides the ldapsearch,
ldapmodify, and ldapdelete tools that use the
Mozilla LDAP C SDK libraries.


%package devel
Summary:          Development libraries and examples for Mozilla LDAP C SDK
Group:            Development/Libraries
Requires:         mozldap = %{version}-%{release}
Requires:         nspr-devel >= %{nspr_version}, nss-devel >= %{nspr_version}
Provides:         mozldap-devel

%description devel
Header and Library files for doing development with Network Security Services.

%prep
%setup -q
%ifarch x86_64 ppc64 ia64 s390x
arg64="--enable-64bit"
%endif
cd mozilla/directory/c-sdk
./configure $arg64 --with-nss --system-svrcore --enable-optimize --disable-debug

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
%{__cat} mozldap.pc.in | sed -e "s,%%libdir%%,%{_libdir},g" \
                          -e "s,%%prefix%%,%{_prefix},g" \
                          -e "s,%%exec_prefix%%,%{_prefix},g" \
                          -e "s,%%includedir%%,%{_includedir}/mozldap,g" \
                          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
                          -e "s,%%NSS_VERSION%%,%{nss_version},g" > \
                          -e "s,%%MOZLDAP_VERSION%%,%{version},g" > \
                          $RPM_BUILD_ROOT/%{_libdir}/pkgconfig/mozldap.pc

%install

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT/%{_includedir}/mozldap
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}
%{__mkdir_p} $RPM_BUILD_ROOT/%{_libdir}/mozldap

# Copy the binary libraries we want
for file in libssldap50.so libprldap50.so libldap50.so
do
  %{__install} -m 755 mozilla/dist/lib/$file $RPM_BUILD_ROOT/%{_libdir}
  mv $RPM_BUILD_ROOT/%{_libdir}/$file $RPM_BUILD_ROOT/%{_libdir}/$file.%{major}.%{minor}
  ln -s $RPM_BUILD_ROOT/%{_libdir}/$file.%{major}.%{minor} $RPM_BUILD_ROOT/%{_libdir}/$file.%{major}
  ln -s $RPM_BUILD_ROOT/%{_libdir}/$file.%{major} $RPM_BUILD_ROOT/%{_libdir}/$file
done
# Rename the libraries and create the symlinks


# Copy the binaries we want
for file in ldapsearch ldapmodify ldapdelete ldapcmp ldapcompare
do
  %{__install} -m 755 mozilla/dist/bin/$file $RPM_BUILD_ROOT/%{_libdir}/mozldap
done

# Copy the include files
for file in mozilla/dist/public/ldap/*.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT/%{_includedir}/mozldap
done

# Copy the developer files
%{__mkdir_p} $RPM_BUILD_ROOT/usr/share/mozldap
cp -r mozilla/directory/c-sdk/ldap/examples $RPM_BUILD_ROOT/usr/share/mozldap
%{__mkdir_p} $RPM_BUILD_ROOT/usr/share/mozldap/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/examples/xmplflt.conf $RPM_BUILD_ROOT/usr/share/mozldap/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldaptemplates.conf $RPM_BUILD_ROOT/usr/share/mozldap/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapfilter.conf $RPM_BUILD_ROOT/usr/share/mozldap/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapsearchprefs.conf $RPM_BUILD_ROOT/usr/share/mozldap/etc

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
%{_libdir}/mozldap/ldapsearch
%{_libdir}/mozldap/ldapmodify
%{_libdir}/mozldap/ldapdelete
%{_libdir}/mozldap/ldapcmp
%{_libdir}/mozldap/ldapcompare

%files devel
%defattr(0644,root,root)
%{_libdir}/pkgconfig/mozldap.pc
%{_includedir}/mozldap
/usr/share/mozldap

%changelog
* Tue Jan 31 2006 Rich Megginson <rmeggins@redhat.com> - 5.17-2
- use --system-svrcore to configure

* Mon Dec 19 2005 Rich Megginson <rmeggins@redhat.com> - 5.17-1
- Initial revision

