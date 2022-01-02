if (-not (test-path env:MOZILLABUILD)) {
  echo "No MOZILLABUILD environment variable found, terminating."
  exit 1
}

$mypath = $MyInvocation.MyCommand.Path
$machpath = ($mypath -replace '\\', '/').substring(0, $mypath.length - 4)

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

& "$env:MOZILLABUILD/start-shell.bat" $machpath $args
exit $lastexitcode
