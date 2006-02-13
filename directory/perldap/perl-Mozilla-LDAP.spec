Summary: LDAP Perl module that wraps the Mozilla C SDK
Name: perl-Mozilla-LDAP
Version: 1.5
Release: 1
License: GPL or Artistic
Group: Development/Libraries
URL: http://www.mozilla.org/directory/perldap.html
BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root
BuildArch: noarch
BuildRequires: perl >= 2:5.8.0
Requires: %(perl -MConfig -le 'if (defined $Config{useithreads}) { print "perl(:WITH_ITHREADS)" } else { print "perl(:WITHOUT_ITHREADS)" }')
Requires: %(perl -MConfig -le 'if (defined $Config{usethreads}) { print "perl(:WITH_THREADS)" } else { print "perl(:WITHOUT_THREADS)" }')
Requires: %(perl -MConfig -le 'if (defined $Config{uselargefiles}) { print "perl(:WITH_LARGEFILES)" } else { print "perl(:WITHOUT_LARGEFILES)" }')
Requires: mozldap >= 5.17, nspr >= 4.6, nss >= 3.11
BuildRequires: mozldap-devel >= 5.17, nspr-devel >= 4.6, nss-devel >= 3.11
Source0: perl-mozldap-1.5.tar.gz

%description
%{summary}.

%prep
%setup -q -n perl-mozldap-%{version}

%build

# first, get the locations of the ldap c sdk, nss, and nspr
LDAPSDKINCDIR=`/usr/bin/pkg-config --cflags-only-I mozldap | sed 's/-I//'`
LDAPSDKLIBDIR=`/usr/bin/pkg-config --libs-only-L mozldap | sed 's/-L//'`

# get nspr locations
NSPRINCDIR=`/usr/bin/pkg-config --cflags-only-I nspr | sed 's/-I//'`
NSPRLIBDIR=`/usr/bin/pkg-config --libs-only-L nspr | sed 's/-L//'`
LDAPPR=Y

# get nss locations
NSSLIBDIR=`/usr/bin/pkg-config --libs-only-L nss | sed 's/-L//'`
LDAPSDKSSL=Y

export LDAPSDKINCDIR LDAPSDKLIBDIR LDAPSDKSSL LDAPPR NSPRINCDIR NSPRLIBDIR NSSLIBDIR
CFLAGS="$RPM_OPT_FLAGS" perl Makefile.PL PREFIX=$RPM_BUILD_ROOT%{_prefix} INSTALLDIRS=vendor < /dev/null
make OPTIMIZE="$RPM_OPT_FLAGS" CFLAGS="$RPM_OPT_FLAGS" 
make test

%install
rm -rf $RPM_BUILD_ROOT
eval `perl '-V:installarchlib'`
mkdir -p $RPM_BUILD_ROOT$installarchlib
%makeinstall
rm -f `find $RPM_BUILD_ROOT -type f -name perllocal.pod -o -name .packlist`

[ -x %{_libdir}/rpm/brp-compress ] && %{_libdir}/rpm/brp-compress

find $RPM_BUILD_ROOT%{_prefix} -type f -print | \
	sed "s@^$RPM_BUILD_ROOT@@g" > %{name}-%{version}-%{release}-filelist
if [ "$(cat %{name}-%{version}-%{release}-filelist)X" = "X" ] ; then
    echo "ERROR: EMPTY FILE LIST"
    exit 1
fi

%clean
rm -rf $RPM_BUILD_ROOT

%files -f %{name}-%{version}-%{release}-filelist
%defattr(-,root,root,-)
%doc CREDITS ChangeLog README TODO

%changelog
* Tue Feb  7 2006 Rich Megginson <richm@stanfordalumni.org> - 1.5-1
- Based on the perl-LDAP.spec file

