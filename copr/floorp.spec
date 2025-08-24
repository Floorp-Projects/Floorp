%global name floorp
%global debug_package %{nil}

Name: floorp
Version: 12.0.6
Release: 1%{?dist}

License: MPLv2
Summary: A Firefox-based, flexible browser developed in Japan with a variety of features
Url: https://floorp.app/

Source0: https://github.com/Floorp-Projects/Floorp/releases/download/v%{version}/floorp-linux-amd64.tar.xz
Source1: floorp.desktop
Source2: policies.json
Source3: floorp

ExclusiveArch: x86_64

%description
Floorp is a fork of Firefox ESR with additional features aimed to make the
overall Firefox Browser experince better than vanilla Firefox.

See the latest changelog at: https://blog.floorp.app/

%prep
%setup -q -n %{name}

%install
%__rm -rf %{buildroot}

%__install -d %{buildroot}{/opt/%{name},%{_bindir},%{_datadir}/applications,%{_datadir}/icons/hicolor/128x128/apps,%{_datadir}/icons/hicolor/64x64/apps,%{_datadir}/icons/hicolor/48x48/apps,%{_datadir}/icons/hicolor/32x32/apps,%{_datadir}/icons/hicolor/16x16/apps}

%__cp -r * %{buildroot}/opt/%{name}

%__install -D -m 0644 %{SOURCE1} -t %{buildroot}%{_datadir}/applications

%__install -D -m 0444 %{SOURCE2} -t %{buildroot}/opt/%{name}/distribution

%__install -D -m 0755 %{SOURCE3} -t %{buildroot}%{_bindir}

%__ln_s ../../../../../../opt/%{name}/browser/chrome/icons/default/default128.png %{buildroot}%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%__ln_s ../../../../../../opt/%{name}/browser/chrome/icons/default/default64.png %{buildroot}%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%__ln_s ../../../../../../opt/%{name}/browser/chrome/icons/default/default48.png %{buildroot}%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%__ln_s ../../../../../../opt/%{name}/browser/chrome/icons/default/default32.png %{buildroot}%{_datadir}/icons/hicolor/32x32/apps/%{name}.png
%__ln_s ../../../../../../opt/%{name}/browser/chrome/icons/default/default16.png %{buildroot}%{_datadir}/icons/hicolor/16x16/apps/%{name}.png

%files
%{_datadir}/applications/%{name}.desktop
%{_datadir}/icons/hicolor/128x128/apps/%{name}.png
%{_datadir}/icons/hicolor/64x64/apps/%{name}.png
%{_datadir}/icons/hicolor/48x48/apps/%{name}.png
%{_datadir}/icons/hicolor/32x32/apps/%{name}.png
%{_datadir}/icons/hicolor/16x16/apps/%{name}.png
%{_bindir}/%{name}
/opt/%{name}
