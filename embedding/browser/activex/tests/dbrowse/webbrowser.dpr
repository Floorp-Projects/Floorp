program webbrowser;

uses
  Forms,
  SHDocVw_TLB in '..\..\Imports\SHDocVw_TLB.pas',
  form in 'form.pas' {MainForm};

{$R *.RES}

begin
  Application.Initialize;
  Application.CreateForm(TMainForm, MainForm);
  Application.Run;
end.
