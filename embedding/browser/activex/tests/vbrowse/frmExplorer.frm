VERSION 5.00
Object = "{EAB22AC0-30C1-11CF-A7EB-0000C05BAE0B}#1.1#0"; "SHDOCVW.DLL"
Begin VB.Form frmExplorer 
   Caption         =   "Internet Explorer Control"
   ClientHeight    =   5550
   ClientLeft      =   60
   ClientTop       =   345
   ClientWidth     =   6675
   LinkTopic       =   "Form1"
   ScaleHeight     =   5550
   ScaleWidth      =   6675
   StartUpPosition =   3  'Windows Default
   Begin SHDocVwCtl.WebBrowser Browser1 
      Height          =   3015
      Left            =   0
      TabIndex        =   0
      Top             =   0
      Width           =   4575
      ExtentX         =   8070
      ExtentY         =   5318
      ViewMode        =   1
      Offline         =   0
      Silent          =   0
      RegisterAsBrowser=   0
      RegisterAsDropTarget=   1
      AutoArrange     =   -1  'True
      NoClientEdge    =   0   'False
      AlignLeft       =   0   'False
      ViewID          =   "{0057D0E0-3573-11CF-AE69-08002B2E1262}"
      Location        =   ""
   End
End
Attribute VB_Name = "frmExplorer"
Attribute VB_GlobalNameSpace = False
Attribute VB_Creatable = False
Attribute VB_PredeclaredId = True
Attribute VB_Exposed = False
Private Sub Browser1_NavigateComplete(ByVal URL As String)
    frmToolBar.Browser_NavigateComplete URL
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

Private Sub Browser1_BeforeNavigate2(ByVal pDisp As Object, URL As Variant, Flags As Variant, TargetFrameName As Variant, PostData As Variant, Headers As Variant, Cancel As Boolean)
    frmToolBar.Browser_BeforeNavigate2 pDisp, URL, Flags, TargetFrameName, PostData, Headers, Cancel
End Sub

Private Sub Browser1_NavigateComplete2(ByVal pDisp As Object, URL As Variant)
    frmToolBar.Browser_NavigateComplete2 pDisp, URL
End Sub


