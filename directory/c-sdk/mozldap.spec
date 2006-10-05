%define nspr_name       nspr
%define nspr_version    4.6
%define nss_name        nss
%define nss_version     3.11
%define svrcore_name    svrcore-devel
%define svrcore_version 4.0.1
%define major           6
%define minor           0
%define submin          0
%define libsuffix       %{major}0
%define myname          mozldap%{major}
%define incdir          %{_includedir}/%{myname}
%define mybindir        %{_libdir}/%{myname}
%define mydatadir       %{_datadir}/%{myname}

Summary:          Mozilla LDAP C SDK
Name:             %{myname}
Version:          %{major}.%{minor}.%{submin}
Release:          1%{?dist}
License:          MPL/GPL/LGPL
URL:              http://www.mozilla.org/directory/csdk.html
Group:            System Environment/Libraries
Requires:         %{nspr_name} >= %{nspr_version}
Requires:         %{nss_name} >= %{nss_version}
BuildRoot:        %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
BuildRequires:    %{nspr_name}-devel >= %{nspr_version}
BuildRequires:    %{nss_name}-devel >= %{nss_version}
BuildRequires:    %{svrcore_name} >= %{svrcore_version}

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
BuildRequires:    %{svrcore_name} >= %{svrcore_version}

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

%description devel
Header and Library files for doing development with the Mozilla LDAP C SDK

%prep
%setup -q
%ifarch x86_64 ppc64 ia64 s390x
arg64="--enable-64bit"
%endif
cd mozilla/directory/c-sdk
%configure $arg64 --with-sasl --enable-clu --with-system-svrcore --enable-optimize --disable-debug

%build
if [ $RPM_BUILD_ROOT != "/" ] ; then %{__rm} -rf $RPM_BUILD_ROOT ; fi

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

%ifarch x86_64 ppc64 ia64 s390x
USE_64=1
export USE_64
%endif

cd mozilla/directory/c-sdk
make

%install
if [ $RPM_BUILD_ROOT != "/" ] ; then %{__rm} -rf $RPM_BUILD_ROOT ; fi

# Set up our package file
%{__mkdir_p} $RPM_BUILD_ROOT%{_libdir}/pkgconfig
%{__cat} mozilla/directory/c-sdk/mozldap.pc.in | sed -e "s,%%libdir%%,%{_libdir},g" \
                          -e "s,%%prefix%%,%{_prefix},g" \
                          -e "s,%%major%%,%{major},g" \
                          -e "s,%%minor%%,%{minor},g" \
                          -e "s,%%submin%%,%{submin},g" \
                          -e "s,%%libsuffix%%,%{libsuffix},g" \
                          -e "s,%%bindir%%,%{mybindir},g" \
                          -e "s,%%exec_prefix%%,%{_prefix},g" \
                          -e "s,%%includedir%%,%{incdir},g" \
                          -e "s,%%NSPR_VERSION%%,%{nspr_version},g" \
                          -e "s,%%NSS_VERSION%%,%{nss_version},g" \
                          -e "s,%%SVRCORE_VERSION%%,%{svrcore_version},g" \
                          -e "s,%%MOZLDAP_VERSION%%,%{version},g" > \
                          $RPM_BUILD_ROOT%{_libdir}/pkgconfig/%{name}.pc

# There is no make install target so we'll do it ourselves.

%{__mkdir_p} $RPM_BUILD_ROOT%{incdir}
%{__mkdir_p} $RPM_BUILD_ROOT%{mybindir}

# Copy the binary libraries we want
for file in libssldap%{libsuffix}.so libprldap%{libsuffix}.so libldap%{libsuffix}.so
do
  %{__install} -m 755 mozilla/dist/lib/$file $RPM_BUILD_ROOT%{_libdir}
done

# Copy the binaries we want
for file in ldapsearch ldapmodify ldapdelete ldapcmp ldapcompare ldappasswd
do
  %{__install} -m 755 mozilla/dist/bin/$file $RPM_BUILD_ROOT%{mybindir}
done

# Copy the include files
for file in mozilla/dist/public/ldap/*.h
do
  %{__install} -m 644 $file $RPM_BUILD_ROOT%{incdir}
done

# Copy the developer files
%{__mkdir_p} $RPM_BUILD_ROOT%{mydatadir}
cp -r mozilla/directory/c-sdk/ldap/examples $RPM_BUILD_ROOT%{mydatadir}
%{__mkdir_p} $RPM_BUILD_ROOT%{mydatadir}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/examples/xmplflt.conf $RPM_BUILD_ROOT%{mydatadir}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldaptemplates.conf $RPM_BUILD_ROOT%{mydatadir}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapfilter.conf $RPM_BUILD_ROOT%{mydatadir}/etc
%{__install} -m 644 mozilla/directory/c-sdk/ldap/libraries/libldap/ldapsearchprefs.conf $RPM_BUILD_ROOT%{mydatadir}/etc

# Rename the libraries and create the symlinks
cd $RPM_BUILD_ROOT%{_libdir}
for file in libssldap%{libsuffix}.so libprldap%{libsuffix}.so libldap%{libsuffix}.so
do
  mv $file $file.%{major}.%{minor}.%{submin}
  ln -s $file.%{major}.%{minor}.%{submin} $file.%{major}.%{minor}
  ln -s $file.%{major}.%{minor}.%{submin} $file.%{major}
  ln -s $file.%{major}.%{minor}.%{submin} $file
done

%clean
if [ $RPM_BUILD_ROOT != "/" ] ; then %{__rm} -rf $RPM_BUILD_ROOT ; fi


%post -p /sbin/ldconfig


%postun -p /sbin/ldconfig


%files
%defattr(-,root,root,-)
%doc mozilla/directory/c-sdk/README.rpm
%{_libdir}/libssldap%{libsuffix}.so.%{major}
%{_libdir}/libprldap%{libsuffix}.so.%{major}
%{_libdir}/libldap%{libsuffix}.so.%{major}
%{_libdir}/libssldap%{libsuffix}.so.%{major}.%{minor}
%{_libdir}/libprldap%{libsuffix}.so.%{major}.%{minor}
%{_libdir}/libldap%{libsuffix}.so.%{major}.%{minor}
%{_libdir}/libssldap%{libsuffix}.so.%{major}.%{minor}.%{submin}
%{_libdir}/libprldap%{libsuffix}.so.%{major}.%{minor}.%{submin}
%{_libdir}/libldap%{libsuffix}.so.%{major}.%{minor}.%{submin}

%files tools
%defattr(-,root,root,-)
%dir %{mybindir}
%{mybindir}/ldapsearch
%{mybindir}/ldapmodify
%{mybindir}/ldapdelete
%{mybindir}/ldapcmp
%{mybindir}/ldapcompare
%{mybindir}/ldappasswd

%files devel
%defattr(-,root,root,-)
%{_libdir}/libssldap%{libsuffix}.so
%{_libdir}/libprldap%{libsuffix}.so
%{_libdir}/libldap%{libsuffix}.so
%{_libdir}/pkgconfig/%{name}.pc
%{incdir}
%{mydatadir}

%changelog
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

