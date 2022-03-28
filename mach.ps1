$mypath = $MyInvocation.MyCommand.Path
$machpath = $mypath.substring(0, $mypath.length - 4)

if (Get-Command py -ErrorAction SilentlyContinue) {
  $python_executable = "py"
} else {
  $python_executable = "python"
}

if (-not (test-path env:MACH_PS1_USE_MOZILLABUILD)) {
  &$python_executable $machpath $args
  exit $lastexitcode
}

if (-not (test-path env:MOZILLABUILD)) {
  echo "No MOZILLABUILD environment variable found, terminating."
  exit 1
}

$machpath = ($machpath -replace '\\', '/')

if ($machpath.contains(' ')) {
  echo @'
The repository path contains whitespace which currently isn't supported in mach.ps1.
Please run MozillaBuild manually for now.
'@
  exit 1
}

for ($i = 0; $i -lt $args.length; $i++) {
  $arg = $args[$i]
  if ($arg.contains(' ')) {
    echo @'
The command contains whitespace which currently isn't supported in mach.ps1.
Please run MozillaBuild manually for now.
'@
    exit 1
  }
}

$mozillabuild_version = Get-Content "$env:MOZILLABUILD\VERSION"
# Remove "preX" postfix if the current MozillaBuild is a prerelease.
$mozillabuild_version = [decimal]($mozillabuild_version -replace "pre.*")

if ($mozillabuild_version -ge 4.0) {
  & "$env:MOZILLABUILD/start-shell.bat" -no-start -defterm -c "$machpath $args"
} else {
  & "$env:MOZILLABUILD/start-shell.bat" $machpath $args
}
exit $lastexitcode
