unit form;

interface

uses
  Windows, Messages, SysUtils, Classes, Graphics, Controls, Forms, Dialogs,
  StdCtrls, OleCtrls, SHDocVw_TLB, MOZILLACONTROLLib_TLB, ExtCtrls,
  ComCtrls;

type
  TMainForm = class(TForm)
    Browser: TMozillaBrowser;
    Panel: TPanel;
    Status: TPanel;
    Address: TComboBox;
    Go: TButton;
    WebProgress: TProgressBar;
    Stop: TButton;
    procedure GoClick(Sender: TObject);
    procedure FormResize(Sender: TObject);
    procedure OnStatusTextChange(Sender: TObject; const Text: WideString);
    procedure OnProgressChange(Sender: TObject; Progress,
      ProgressMax: Integer);
    procedure StopClick(Sender: TObject);
  private
    { Private declarations }
  public
    { Public declarations }
  end;

var
  MainForm: TMainForm;

implementation

{$R *.DFM}

procedure TMainForm.GoClick(Sender: TObject);
var Flags, TargetFrameName, PostData, Headers: OleVariant;
begin
     Flags := 0;
     Browser.Navigate(Address.Text, Flags, TargetFrameName, PostData, Headers);
end;

procedure TMainForm.FormResize(Sender: TObject);
var
   oldPanelWidth : Integer;
begin
     oldPanelWidth := Panel.Width;
     Panel.Top :=  MainForm.ClientHeight - Panel.Height;
     Panel.Width := MainForm.ClientWidth;
     Go.Left := Go.Left + Panel.Width - oldPanelWidth;
     WebProgress.Left := WebProgress.Left + Panel.Width - oldPanelWidth;
     Address.Width := Address.Width + Panel.Width - oldPanelWidth;
     Status.Width := Status.Width + Panel.Width - oldPanelWidth;
     Browser.Width := MainForm.ClientWidth;     
     Browser.Height := MainForm.ClientHeight - Panel.Height;
end;

procedure TMainForm.OnStatusTextChange(Sender: TObject;
  const Text: WideString);
begin
     Status.Caption := Text;
end;

procedure TMainForm.OnProgressChange(Sender: TObject; Progress,
  ProgressMax: Integer);
begin
     if Progress < 0 then
     begin
        WebProgress.Position := 0;
        WebProgress.Max := 100;
     end
     else if ProgressMax < Progress then
     begin
        WebProgress.Position := Progress;
        WebProgress.Max := Progress * 10;
     end
     else
     begin
         WebProgress.Position := Progress;
         WebProgress.Max := ProgressMax;
     end
end;

procedure TMainForm.StopClick(Sender: TObject);
begin
    Browser.Stop();
end;

end.
