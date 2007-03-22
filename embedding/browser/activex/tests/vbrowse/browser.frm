VERSION 5.00
Object = "{1339B53E-3453-11D2-93B9-000000000000}#1.0#0"; "MozillaControl.dll"
Begin VB.Form frmMozilla 
   Caption         =   "Mozilla Control"
   ClientHeight    =   5880
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   7710
   LinkTopic       =   "Form1"
   ScaleHeight     =   392
   ScaleMode       =   3  'Pixel
   ScaleWidth      =   514
   Begin MOZILLACONTROLLibCtl.MozillaBrowser Browser1 
      Height          =   5535
      Left            =   0
      OleObjectBlob   =   "browser.frx":0000
      TabIndex        =   0
      Top             =   0
      Width           =   7695
   End
End
Attribute VB_Name = "frmMozilla"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Browser1_BeforeNavigate(ByVal URL As String, ByVal Flags As Long, ByVal TargetFrameName As String, PostData As Variant, ByVal Headers As String, Cancel As Boolean)
    frmToolBar.Browser_BeforeNavigate URL, Flags, TargetFrameName, PostData, Headers, Cancel
End Sub

Private Sub Browser1_NavigateComplete(ByVal URL As String)
    frmToolBar.Browser_NavigateComplete URL
End Sub

Private Sub Browser1_BeforeNavigate2(ByVal pDisp As Object, URL As Variant, Flags As Variant, TargetFrameName As Variant, PostData As Variant, Headers As Variant, Cancel As Boolean)
    frmToolBar.Browser_BeforeNavigate2 pDisp, URL, Flags, TargetFrameName, PostData, Headers, Cancel
End Sub

Private Sub Browser1_NavigateComplete2(ByVal pDisp As Object, URL As Variant)
    frmToolBar.Browser_NavigateComplete2 pDisp, URL
End Sub

Private Sub Browser1_ProgressChange(ByVal Progress As Long, ByVal ProgressMax As Long)
    frmToolBar.Browser_ProgressChange Progress, ProgressMax
End Sub

Private Sub Browser1_StatusTextChange(ByVal Text As String)
    frmToolBar.Browser_StatusTextChange Text
End Sub

Private Sub Form_Resize()
    Browser1.Width = ScaleWidth
    Browser1.Height = ScaleHeight
End Sub
